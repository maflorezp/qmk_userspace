#include "game_snake.h"

#include <stdio.h>
#include <string.h>

#include "game_blocks.h"
#include "game_font.h"
#include "game_ui.h"

// Mismo tablero que el Tetris: cuadros de 14 px, 12x20, con la franja de puntaje encima
#define FIELD_COLS 12
#define FIELD_ROWS 20
#define CELL_SIZE 14
#define FIELD_CELLS (FIELD_COLS * FIELD_ROWS)
#define FIELD_WIDTH (FIELD_COLS * CELL_SIZE)
#define FIELD_HEIGHT (FIELD_ROWS * CELL_SIZE)
#define FIELD_X ((GAME_SCREEN_WIDTH - FIELD_WIDTH) / 2)
// Franja superior con el puntaje, el largo y el nivel
#define HUD_HEIGHT (GAME_SCREEN_HEIGHT - FIELD_HEIGHT)
#define HUD_X 4

// La culebrita sale horizontal, mirando a la derecha, a media altura
#define START_LENGTH 3
#define START_COL 3
#define START_ROW (FIELD_ROWS / 2)

#define FOOD_PER_LEVEL 5
#define POINTS_PER_FOOD 10
#define STEP_INTERVAL_START_MS 180
#define STEP_INTERVAL_PER_LEVEL_MS 15
#define STEP_INTERVAL_MIN_MS 70
// Giros que se guardan si llegan varios entre dos pasos, así no se pierden las pulsaciones rápidas
#define TURN_QUEUE_SIZE 2
#define MAX_SCORE 999999UL

enum palette_index {
    PALETTE_EMPTY = GAME_BLOCKS_EMPTY,
    PALETTE_BODY,
    PALETTE_HEAD,
    PALETTE_FOOD,
};

static const game_color_t palette[] = {
    [PALETTE_EMPTY] = {0, 0, 22},
    [PALETTE_BODY]  = {85, 255, 200},
    [PALETTE_HEAD]  = {60, 255, 255},
    [PALETTE_FOOD]  = {0, 255, 255},
};

static const game_color_t hud_color = {0, 0, 200};

typedef enum {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
} direction_t;

// Avance de cada dirección, en el orden de direction_t
static const int8_t direction_cols[] = {0, 1, 0, -1};
static const int8_t direction_rows[] = {-1, 0, 1, 0};

typedef enum {
    STATE_READY,
    STATE_PLAYING,
    STATE_OVER,
} snake_state_t;

static snake_state_t state;
// Cuerpo como anillo de posiciones (fila * FIELD_COLS + columna): body[tail] es la cola y la
// cabeza está length - 1 posiciones más adelante
static uint8_t     body[FIELD_CELLS];
static uint8_t     tail;
static uint8_t     length;
// Cuadros ocupados por la culebrita, para saber en un paso si se muerde
static bool        occupied[FIELD_ROWS][FIELD_COLS];
static direction_t direction;
static direction_t turn_queue[TURN_QUEUE_SIZE];
static uint8_t     turn_count;
static uint8_t     food;
static uint16_t    eaten;
static uint32_t    score;
static uint32_t    record;
static uint32_t    step_timer;
static uint32_t    rng_state;

static uint32_t random_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint8_t level(void) {
    return 1 + eaten / FOOD_PER_LEVEL;
}

static uint16_t step_interval(void) {
    int16_t interval = STEP_INTERVAL_START_MS - (level() - 1) * STEP_INTERVAL_PER_LEVEL_MS;
    return interval < STEP_INTERVAL_MIN_MS ? STEP_INTERVAL_MIN_MS : interval;
}

static uint8_t body_at(uint8_t index) {
    return body[(tail + index) % FIELD_CELLS];
}

static uint8_t head_position(void) {
    return body_at(length - 1);
}

static void set_cell(uint8_t position, uint8_t value) {
    game_blocks_set(position % FIELD_COLS, position / FIELD_COLS, value);
}

static void draw_hud(void) {
    char text[16];

    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_1_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
    snprintf(text, sizeof(text), "LAR %03u N%02u", length, level());
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_2_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

// Pone la comida en un cuadro libre al azar; devuelve false si ya no queda ninguno
static bool place_food(void) {
    uint16_t free_cells = FIELD_CELLS - length;
    if (free_cells == 0) {
        return false;
    }
    uint16_t skip = random_next() % free_cells;
    for (uint8_t row = 0; row < FIELD_ROWS; row++) {
        for (uint8_t col = 0; col < FIELD_COLS; col++) {
            if (occupied[row][col]) {
                continue;
            }
            if (skip == 0) {
                food = row * FIELD_COLS + col;
                set_cell(food, PALETTE_FOOD);
                return true;
            }
            skip--;
        }
    }
    return false;
}

static void finish_game(bool won) {
    char text[16];

    state = STATE_OVER;
    if (score > record) {
        record = score;
        game_host_record_save(GAME_ID_SNAKE, record);
    }
    game_blocks_flush();
    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, won ? "GANASTE" : "FIN", text, "CLIC: OTRA");
}

// Toma el siguiente giro pendiente, si hay
static void apply_turn(void) {
    if (turn_count == 0) {
        return;
    }
    direction = turn_queue[0];
    turn_count--;
    if (turn_count > 0) {
        turn_queue[0] = turn_queue[1];
    }
}

// Avanza un cuadro: solo cambian la cabeza nueva, la anterior (pasa a cuerpo), la cola y la comida
static void step(void) {
    apply_turn();

    uint8_t head = head_position();
    int8_t  col  = head % FIELD_COLS + direction_cols[direction];
    int8_t  row  = head / FIELD_COLS + direction_rows[direction];

    // Los bordes matan: no se da la vuelta por el otro lado
    if (col < 0 || col >= FIELD_COLS || row < 0 || row >= FIELD_ROWS) {
        finish_game(false);
        return;
    }

    uint8_t new_head = row * FIELD_COLS + col;
    bool    grows    = new_head == food;

    // Sin crecer, la cola se va en este mismo paso: la cabeza sí puede entrar en su cuadro
    if (occupied[row][col] && (grows || new_head != body[tail])) {
        finish_game(false);
        return;
    }
    if (!grows) {
        uint8_t old_tail = body[tail];
        occupied[old_tail / FIELD_COLS][old_tail % FIELD_COLS] = false;
        set_cell(old_tail, PALETTE_EMPTY);
        tail = (tail + 1) % FIELD_CELLS;
        length--;
    }

    set_cell(head, PALETTE_BODY);
    body[(tail + length) % FIELD_CELLS] = new_head;
    length++;
    occupied[row][col] = true;
    set_cell(new_head, PALETTE_HEAD);

    if (grows) {
        eaten++;
        score += (uint32_t)POINTS_PER_FOOD * level();
        if (score > MAX_SCORE) {
            score = MAX_SCORE;
        }
        draw_hud();
        if (!place_food()) {
            finish_game(true);
            return;
        }
    }
    game_blocks_flush();
}

// Reparte la culebrita inicial en memoria (no dibuja)
static void reset_snake(void) {
    memset(occupied, 0, sizeof(occupied));
    tail       = 0;
    length     = START_LENGTH;
    direction  = DIRECTION_RIGHT;
    turn_count = 0;
    for (uint8_t i = 0; i < START_LENGTH; i++) {
        body[i]                            = START_ROW * FIELD_COLS + START_COL + i;
        occupied[START_ROW][START_COL + i] = true;
    }
}

// Pasa al tablero la culebrita completa
static void render_snake(void) {
    for (uint8_t i = 0; i < length; i++) {
        set_cell(body_at(i), i == length - 1 ? PALETTE_HEAD : PALETTE_BODY);
    }
}

static void start_game(uint32_t now_ms) {
    state      = STATE_PLAYING;
    score      = 0;
    eaten      = 0;
    step_timer = now_ms;
    reset_snake();

    draw_hud();
    // El recuadro tapó parte del tablero: se borra y se redibuja completo
    game_host_fill_rect(0, HUD_HEIGHT, GAME_SCREEN_WIDTH, FIELD_HEIGHT, GAME_COLOR_BLACK);
    game_blocks_clear();
    game_blocks_invalidate();
    render_snake();
    place_food();
    game_blocks_flush();
}

void game_snake_init(uint32_t seed, uint32_t now_ms) {
    static const game_blocks_config_t config = {
        .origin_x  = FIELD_X,
        .origin_y  = HUD_HEIGHT,
        .cols      = FIELD_COLS,
        .rows      = FIELD_ROWS,
        .cell_size = CELL_SIZE,
        .palette   = palette,
    };

    rng_state  = seed != 0 ? seed : 1;
    record     = game_host_record_load(GAME_ID_SNAKE);
    state      = STATE_READY;
    score      = 0;
    eaten      = 0;
    step_timer = now_ms;
    reset_snake();

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    game_blocks_init(&config);
    draw_hud();
    render_snake();
    game_blocks_flush();

    char text[16];
    snprintf(text, sizeof(text), "REC %06lu", (unsigned long)record);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, "CULEBRITA", text, "CLIC: JUGAR");
}

void game_snake_input(game_input_t input, uint32_t now_ms) {
    if (state != STATE_PLAYING) {
        if (input == GAME_INPUT_ACTION) {
            start_game(now_ms);
        }
        return;
    }

    direction_t wanted;
    switch (input) {
        case GAME_INPUT_UP:
            wanted = DIRECTION_UP;
            break;
        case GAME_INPUT_RIGHT:
            wanted = DIRECTION_RIGHT;
            break;
        case GAME_INPUT_DOWN:
            wanted = DIRECTION_DOWN;
            break;
        case GAME_INPUT_LEFT:
            wanted = DIRECTION_LEFT;
            break;
        default:
            return;
    }

    // Se compara con el último giro pendiente: repetir la dirección o darse la vuelta sobre sí
    // misma no hace nada (las direcciones opuestas difieren en 2)
    direction_t last = turn_count > 0 ? turn_queue[turn_count - 1] : direction;
    if (wanted == last || (wanted + 2) % 4 == last || turn_count == TURN_QUEUE_SIZE) {
        return;
    }
    turn_queue[turn_count++] = wanted;
}

void game_snake_tick(uint32_t now_ms) {
    if (state == STATE_PLAYING && now_ms - step_timer >= step_interval()) {
        step_timer = now_ms;
        step();
    }
}
