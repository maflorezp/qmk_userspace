#include "game_car.h"

#include <stdio.h>

#include "game_blocks.h"
#include "game_font.h"

// Tablero: un muro a cada lado y tres carriles de tres cuadros en medio
#define LANE_COUNT 3
#define LANE_WIDTH 3
#define FIELD_COLS (LANE_COUNT * LANE_WIDTH + 2)
#define FIELD_ROWS 20
#define CELL_SIZE 14
#define FIELD_WIDTH (FIELD_COLS * CELL_SIZE)
#define FIELD_X ((GAME_SCREEN_WIDTH - FIELD_WIDTH) / 2)
// Franja superior con el puntaje, el récord y el nivel
#define HUD_HEIGHT (GAME_SCREEN_HEIGHT - FIELD_ROWS * CELL_SIZE)
#define HUD_TEXT_SCALE 2
#define HUD_LINE_1_Y 4
#define HUD_LINE_2_Y 22

#define CAR_WIDTH 3
#define CAR_HEIGHT 4
#define PLAYER_ROW (FIELD_ROWS - CAR_HEIGHT)
#define START_LANE 1
#define MAX_ENEMIES 8

// Filas entre una tanda de carros y la siguiente. Con 9 quedan 5 filas libres entre tandas: el
// carro (4 filas) siempre tiene un momento para cambiar de carril sin tocar a nadie.
#define WAVE_SPACING 9
// Probabilidad de que una tanda traiga dos carros: crece con el nivel hasta un tope
#define DOUBLE_WAVE_BASE_PERCENT 10
#define DOUBLE_WAVE_PERCENT_PER_LEVEL 8
#define DOUBLE_WAVE_MAX_PERCENT 60

#define CARS_PER_LEVEL 8
#define STEP_INTERVAL_START_MS 130
#define STEP_INTERVAL_PER_LEVEL_MS 9
#define STEP_INTERVAL_MIN_MS 50

// El muro alterna 3 cuadros encendidos y 1 apagado; al desplazarse da la sensación de velocidad
#define WALL_PATTERN_LENGTH 4
#define WALL_PATTERN_GAP 3

#define CRASH_BLINK_INTERVAL_MS 120
#define CRASH_BLINK_COUNT 8

#define OVERLAY_WIDTH 146
#define OVERLAY_HEIGHT 84
#define OVERLAY_BORDER 2
#define OVERLAY_TEXT_SCALE 2
#define OVERLAY_LINE_HEIGHT 22
#define OVERLAY_PADDING 12

enum palette_index {
    PALETTE_EMPTY = GAME_BLOCKS_EMPTY,
    PALETTE_WALL,
    PALETTE_PLAYER,
    PALETTE_ENEMY_FIRST,
    PALETTE_ENEMY_COUNT = 4,
};

static const game_color_t palette[] = {
    [PALETTE_EMPTY]           = {0, 0, 22},
    [PALETTE_WALL]            = {0, 0, 150},
    [PALETTE_PLAYER]          = {128, 255, 255},
    [PALETTE_ENEMY_FIRST]     = {0, 255, 255},
    [PALETTE_ENEMY_FIRST + 1] = {21, 255, 255},
    [PALETTE_ENEMY_FIRST + 2] = {43, 255, 255},
    [PALETTE_ENEMY_FIRST + 3] = {213, 255, 255},
};

static const game_color_t hud_color     = {0, 0, 200};
static const game_color_t overlay_color = {43, 255, 255};

// Silueta del carro, una fila por elemento; el bit 2 es la columna izquierda
static const uint8_t car_shape[CAR_HEIGHT] = {0b010, 0b111, 0b010, 0b101};

typedef enum {
    STATE_READY,
    STATE_PLAYING,
    STATE_CRASHED,
    STATE_OVER,
} car_state_t;

typedef struct {
    bool    active;
    uint8_t lane;
    // Fila de la parte de arriba del carro; negativa mientras entra por el borde superior
    int8_t  top_row;
    uint8_t color;
} enemy_t;

static car_state_t state;
static enemy_t     enemies[MAX_ENEMIES];
static uint8_t     player_lane;
static bool        player_visible;
static uint16_t    score;
static uint16_t    record;
static uint8_t     wall_offset;
static uint8_t     rows_since_wave;
static uint8_t     blinks_left;
static uint32_t    step_timer;
static uint32_t    rng_state;

static uint32_t random_next(void) {
    // xorshift32: suficiente para repartir el tráfico
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint8_t level(void) {
    return 1 + score / CARS_PER_LEVEL;
}

static uint16_t step_interval(void) {
    int16_t interval = STEP_INTERVAL_START_MS - (level() - 1) * STEP_INTERVAL_PER_LEVEL_MS;
    return interval < STEP_INTERVAL_MIN_MS ? STEP_INTERVAL_MIN_MS : interval;
}

static void draw_hud(void) {
    char text[12];

    snprintf(text, sizeof(text), "PTS %04u", score);
    game_font_draw_text(FIELD_X, HUD_LINE_1_Y, text, HUD_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
    snprintf(text, sizeof(text), "REC %04u", record);
    game_font_draw_text(FIELD_X, HUD_LINE_2_Y, text, HUD_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);

    snprintf(text, sizeof(text), "N%02u", level());
    int16_t level_x = FIELD_X + FIELD_WIDTH - game_font_text_width(text, HUD_TEXT_SCALE);
    game_font_draw_text(level_x, HUD_LINE_1_Y, text, HUD_TEXT_SCALE, palette[PALETTE_PLAYER], GAME_COLOR_BLACK);
}

static void put_car(uint8_t lane, int8_t top_row, uint8_t color) {
    int8_t left_col = 1 + lane * LANE_WIDTH;
    for (uint8_t row = 0; row < CAR_HEIGHT; row++) {
        for (uint8_t col = 0; col < CAR_WIDTH; col++) {
            if (car_shape[row] & (1 << (CAR_WIDTH - 1 - col))) {
                game_blocks_set(left_col + col, top_row + row, color);
            }
        }
    }
}

static void render_field(void) {
    game_blocks_clear();

    for (uint8_t row = 0; row < FIELD_ROWS; row++) {
        if ((row + WALL_PATTERN_LENGTH - wall_offset) % WALL_PATTERN_LENGTH != WALL_PATTERN_GAP) {
            game_blocks_set(0, row, PALETTE_WALL);
            game_blocks_set(FIELD_COLS - 1, row, PALETTE_WALL);
        }
    }
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            put_car(enemies[i].lane, enemies[i].top_row, enemies[i].color);
        }
    }
    if (player_visible) {
        put_car(player_lane, PLAYER_ROW, PALETTE_PLAYER);
    }

    game_blocks_flush();
}

// Recuadro con hasta tres líneas centradas sobre el tablero
static void draw_overlay(const char *line_1, const char *line_2, const char *line_3) {
    int16_t x = (GAME_SCREEN_WIDTH - OVERLAY_WIDTH) / 2;
    int16_t y = HUD_HEIGHT + (FIELD_ROWS * CELL_SIZE - OVERLAY_HEIGHT) / 2;

    game_host_fill_rect(x, y, OVERLAY_WIDTH, OVERLAY_HEIGHT, overlay_color);
    game_host_fill_rect(x + OVERLAY_BORDER, y + OVERLAY_BORDER, OVERLAY_WIDTH - 2 * OVERLAY_BORDER, OVERLAY_HEIGHT - 2 * OVERLAY_BORDER, GAME_COLOR_BLACK);

    const char *lines[] = {line_1, line_2, line_3};
    for (uint8_t i = 0; i < 3; i++) {
        int16_t      text_x = (GAME_SCREEN_WIDTH - game_font_text_width(lines[i], OVERLAY_TEXT_SCALE)) / 2;
        game_color_t color  = i == 0 ? overlay_color : GAME_COLOR_WHITE;
        game_font_draw_text(text_x, y + OVERLAY_PADDING + i * OVERLAY_LINE_HEIGHT, lines[i], OVERLAY_TEXT_SCALE, color, GAME_COLOR_BLACK);
    }
}

static bool player_hits_enemy(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].lane == player_lane && enemies[i].top_row + CAR_HEIGHT > PLAYER_ROW && enemies[i].top_row < PLAYER_ROW + CAR_HEIGHT) {
            return true;
        }
    }
    return false;
}

static void spawn_enemy(uint8_t lane) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i] = (enemy_t){
                .active  = true,
                .lane    = lane,
                .top_row = -CAR_HEIGHT,
                .color   = PALETTE_ENEMY_FIRST + random_next() % PALETTE_ENEMY_COUNT,
            };
            return;
        }
    }
}

// Una tanda trae uno o dos carros, nunca tres: siempre queda un carril libre
static void spawn_wave(void) {
    uint8_t first_lane = random_next() % LANE_COUNT;
    spawn_enemy(first_lane);

    uint8_t double_percent = DOUBLE_WAVE_BASE_PERCENT + level() * DOUBLE_WAVE_PERCENT_PER_LEVEL;
    if (double_percent > DOUBLE_WAVE_MAX_PERCENT) {
        double_percent = DOUBLE_WAVE_MAX_PERCENT;
    }
    if (random_next() % 100 < double_percent) {
        spawn_enemy((first_lane + 1 + random_next() % (LANE_COUNT - 1)) % LANE_COUNT);
    }
}

static void start_crash(uint32_t now_ms) {
    state       = STATE_CRASHED;
    blinks_left = CRASH_BLINK_COUNT;
    step_timer  = now_ms;
    if (score > record) {
        record = score;
        game_host_record_save(GAME_ID_CAR, record);
        draw_hud();
    }
}

static void start_game(uint32_t now_ms) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
    }
    state           = STATE_PLAYING;
    player_lane     = START_LANE;
    player_visible  = true;
    score           = 0;
    wall_offset     = 0;
    // La primera tanda sale de inmediato
    rows_since_wave = WAVE_SPACING;
    step_timer      = now_ms;

    draw_hud();
    // El recuadro tapó parte del tablero: se borra (los cuadros no cubren la separación entre
    // ellos) y se redibuja completo
    game_host_fill_rect(FIELD_X, HUD_HEIGHT, FIELD_WIDTH, FIELD_ROWS * CELL_SIZE, GAME_COLOR_BLACK);
    game_blocks_invalidate();
    render_field();
}

static void step(uint32_t now_ms) {
    wall_offset = (wall_offset + 1) % WALL_PATTERN_LENGTH;

    bool scored = false;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            continue;
        }
        enemies[i].top_row++;
        if (enemies[i].top_row >= FIELD_ROWS) {
            enemies[i].active = false;
            score++;
            scored = true;
        }
    }
    if (scored) {
        draw_hud();
    }

    if (++rows_since_wave >= WAVE_SPACING) {
        rows_since_wave = 0;
        spawn_wave();
    }

    if (player_hits_enemy()) {
        start_crash(now_ms);
    }
    render_field();
}

void game_car_init(uint32_t seed, uint32_t now_ms) {
    static const game_blocks_config_t config = {
        .origin_x  = FIELD_X,
        .origin_y  = HUD_HEIGHT,
        .cols      = FIELD_COLS,
        .rows      = FIELD_ROWS,
        .cell_size = CELL_SIZE,
        .palette   = palette,
    };

    // xorshift32 se queda en cero si arranca en cero
    rng_state = seed != 0 ? seed : 1;
    record    = game_host_record_load(GAME_ID_CAR);
    score     = 0;
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = false;
    }
    state          = STATE_READY;
    player_lane    = START_LANE;
    player_visible = true;
    wall_offset    = 0;
    step_timer     = now_ms;

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    game_blocks_init(&config);
    draw_hud();
    render_field();
    draw_overlay("CARRITOS", "GIRA: MOVER", "CLIC: JUGAR");
}

void game_car_input(game_input_t input, uint32_t now_ms) {
    if (state == STATE_READY || state == STATE_OVER) {
        if (input == GAME_INPUT_ACTION) {
            start_game(now_ms);
        }
        return;
    }
    if (state != STATE_PLAYING) {
        return;
    }

    if (input == GAME_INPUT_LEFT && player_lane > 0) {
        player_lane--;
    } else if (input == GAME_INPUT_RIGHT && player_lane < LANE_COUNT - 1) {
        player_lane++;
    } else {
        return;
    }
    // Meterse de lado contra un carro también es choque
    if (player_hits_enemy()) {
        start_crash(now_ms);
    }
    render_field();
}

void game_car_tick(uint32_t now_ms) {
    if (state == STATE_PLAYING) {
        if (now_ms - step_timer >= step_interval()) {
            step_timer = now_ms;
            step(now_ms);
        }
        return;
    }
    if (state != STATE_CRASHED || now_ms - step_timer < CRASH_BLINK_INTERVAL_MS) {
        return;
    }

    // Tras el choque el carro parpadea y luego sale el recuadro con el resultado
    step_timer     = now_ms;
    player_visible = !player_visible;
    render_field();
    if (--blinks_left == 0) {
        char text[12];
        snprintf(text, sizeof(text), "PTS %04u", score);
        state = STATE_OVER;
        draw_overlay("CHOQUE!", text, "CLIC: OTRA");
    }
}
