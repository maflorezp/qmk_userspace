#include "game_breakout.h"

#include <stdio.h>
#include <string.h>

#include "game_font.h"
#include "game_ui.h"

// Franja superior con el puntaje, las vidas y el nivel; debajo, la cancha con un muro de 2 px a
// los lados y arriba
#define HUD_HEIGHT 40
#define HUD_X 4
#define WALL_SIZE 2
#define FIELD_LEFT WALL_SIZE
#define FIELD_RIGHT (GAME_SCREEN_WIDTH - WALL_SIZE)
#define FIELD_TOP (HUD_HEIGHT + WALL_SIZE)
#define FIELD_HEIGHT (GAME_SCREEN_HEIGHT - HUD_HEIGHT)

// Ladrillos: cada uno ocupa una celda y deja libre 1 px a la derecha y 2 px abajo. Los choques se
// miden con la celda completa, así la bola nunca entra en el hueco entre dos ladrillos.
#define BRICK_COLS 8
#define BRICK_ROWS 6
#define BRICK_CELL_WIDTH ((FIELD_RIGHT - FIELD_LEFT) / BRICK_COLS)
#define BRICK_CELL_HEIGHT 10
#define BRICK_WIDTH (BRICK_CELL_WIDTH - 1)
#define BRICK_HEIGHT (BRICK_CELL_HEIGHT - 2)
#define BRICKS_Y (FIELD_TOP + 24)
#define BRICKS_BOTTOM (BRICKS_Y + BRICK_ROWS * BRICK_CELL_HEIGHT)

// Raqueta: cada giro de la rueda o cada tecla la corre un paso de PADDLE_STEP píxeles
#define PADDLE_WIDTH 32
#define PADDLE_HEIGHT 5
#define PADDLE_Y 300
#define PADDLE_STEP 12
#define PADDLE_START_X ((FIELD_LEFT + FIELD_RIGHT - PADDLE_WIDTH) / 2)
// Zonas de la raqueta: según dónde pegue la bola sale con un ángulo distinto
#define PADDLE_ZONES 8

#define BALL_SIZE 4
// La posición de la bola va en punto fijo: 8 bits de fracción
#define FIXED_SHIFT 8
// La bola avanza un paso cada STEP_MS. La rapidez va en dieciseisavos de píxel por paso; el tope
// (4 px por paso) es menor que el alto de un ladrillo, así la bola no atraviesa ninguno.
#define STEP_MS 16
#define SPEED_START 32
#define SPEED_PER_LEVEL 4
#define SPEED_MAX 64
// Si el juego se atrasa más de estos pasos, se pierde el atraso en vez de correr para alcanzarlo
#define MAX_STEP_LAG 4

#define LIVES_START 3
#define MAX_SCORE 999999UL

static const game_color_t wall_color   = {0, 0, 90};
static const game_color_t paddle_color = {128, 255, 255};
static const game_color_t ball_color   = {0, 0, 255};
static const game_color_t hud_color    = {0, 0, 200};

// Un color por fila de ladrillos, de arriba abajo
static const game_color_t brick_colors[BRICK_ROWS] = {
    {0, 255, 255},   // rojo
    {21, 255, 255},  // naranja
    {43, 255, 255},  // amarillo
    {85, 255, 255},  // verde
    {128, 255, 255}, // cian
    {170, 255, 255}, // azul
};

// Puntos por ladrillo según la fila (se multiplican por el nivel): las de arriba valen más
static const uint8_t brick_points[BRICK_ROWS] = {7, 7, 5, 5, 3, 1};

// Dirección de salida en cada zona de la raqueta, como vector de largo 256 (seno y coseno del
// ángulo respecto a la vertical: 60, 45, 30 y 15 grados hacia cada lado)
static const int16_t zone_dx[PADDLE_ZONES] = {-222, -181, -128, -66, 66, 128, 181, 222};
static const int16_t zone_dy[PADDLE_ZONES] = {-128, -181, -222, -247, -247, -222, -181, -128};

typedef enum {
    STATE_READY,
    // La bola espera sobre la raqueta a que el jugador la saque
    STATE_SERVING,
    STATE_PLAYING,
    STATE_OVER,
} breakout_state_t;

static breakout_state_t state;
// Un bit por ladrillo vivo en cada fila
static uint8_t  bricks[BRICK_ROWS];
static uint8_t  bricks_left;
// Filas de ladrillos ya dibujadas al empezar un nivel: se dibuja una por paso para no bloquear
static uint8_t  rows_drawn;
static int16_t  paddle_x;
// Posición y velocidad de la bola en punto fijo; drawn_x/drawn_y es donde quedó pintada
static int32_t  ball_x;
static int32_t  ball_y;
static int32_t  ball_vx;
static int32_t  ball_vy;
static int16_t  drawn_x;
static int16_t  drawn_y;
static bool     ball_visible;
static uint8_t  lives;
static uint8_t  level;
static uint32_t score;
static uint32_t record;
static uint32_t step_timer;
static uint32_t rng_state;

static uint32_t random_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint8_t speed(void) {
    uint16_t value = SPEED_START + (level - 1) * SPEED_PER_LEVEL;
    return value > SPEED_MAX ? SPEED_MAX : value;
}

static int16_t ball_px(void) {
    return ball_x >> FIXED_SHIFT;
}

static int16_t ball_py(void) {
    return ball_y >> FIXED_SHIFT;
}

static bool rects_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void draw_hud_score(void) {
    char text[16];

    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_1_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

static void draw_hud_status(void) {
    char text[16];

    snprintf(text, sizeof(text), "VID %u N%02u", lives, level);
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_2_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

static void draw_walls(void) {
    game_host_fill_rect(0, HUD_HEIGHT, GAME_SCREEN_WIDTH, WALL_SIZE, wall_color);
    game_host_fill_rect(0, FIELD_TOP, WALL_SIZE, GAME_SCREEN_HEIGHT - FIELD_TOP, wall_color);
    game_host_fill_rect(FIELD_RIGHT, FIELD_TOP, WALL_SIZE, GAME_SCREEN_HEIGHT - FIELD_TOP, wall_color);
}

static void draw_brick(uint8_t col, uint8_t row, game_color_t color) {
    game_host_fill_rect(FIELD_LEFT + col * BRICK_CELL_WIDTH, BRICKS_Y + row * BRICK_CELL_HEIGHT, BRICK_WIDTH, BRICK_HEIGHT, color);
}

static void draw_brick_row(uint8_t row) {
    for (uint8_t col = 0; col < BRICK_COLS; col++) {
        if (bricks[row] & (1 << col)) {
            draw_brick(col, row, brick_colors[row]);
        }
    }
}

static void draw_paddle(void) {
    game_host_fill_rect(paddle_x, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, paddle_color);
}

// Borra la bola donde quedó pintada y la pinta donde está ahora. Si al borrarla se llevó un trozo
// de la raqueta (la bola puede pasar a su lado), la raqueta se repinta.
static void redraw_ball(void) {
    if (ball_visible) {
        game_host_fill_rect(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, GAME_COLOR_BLACK);
        if (rects_overlap(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, paddle_x, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT)) {
            draw_paddle();
        }
    }
    drawn_x      = ball_px();
    drawn_y      = ball_py();
    ball_visible = true;
    game_host_fill_rect(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, ball_color);
}

static void erase_ball(void) {
    if (ball_visible) {
        game_host_fill_rect(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, GAME_COLOR_BLACK);
        ball_visible = false;
    }
}

// Deja la bola centrada sobre la raqueta
static void place_ball_on_paddle(void) {
    ball_x = (int32_t)(paddle_x + (PADDLE_WIDTH - BALL_SIZE) / 2) << FIXED_SHIFT;
    ball_y = (int32_t)(PADDLE_Y - BALL_SIZE) << FIXED_SHIFT;
}

static void set_direction(int16_t dx, int16_t dy) {
    // dx y dy van en 1/256 y la rapidez en 1/16 de píxel: el resultado queda en 1/256 de píxel
    ball_vx = ((int32_t)dx * speed()) >> 4;
    ball_vy = ((int32_t)dy * speed()) >> 4;
}

static void fill_bricks(void) {
    for (uint8_t row = 0; row < BRICK_ROWS; row++) {
        bricks[row] = (1 << BRICK_COLS) - 1;
    }
    bricks_left = BRICK_ROWS * BRICK_COLS;
    rows_drawn  = 0;
}

static void serve(void) {
    state = STATE_SERVING;
    place_ball_on_paddle();
    redraw_ball();
}

static void finish_game(void) {
    char text[16];

    state = STATE_OVER;
    erase_ball();
    if (score > record) {
        record = score;
        game_host_record_save(GAME_ID_BREAKOUT, record);
    }
    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, "FIN", text, "CLIC: OTRA");
}

// Rompe los ladrillos que toca la bola en la posición dada; devuelve si rompió alguno. El puntaje
// se repinta una sola vez al final del paso, aunque la bola rompa ladrillos en los dos ejes.
static bool hit_bricks(int16_t x, int16_t y) {
    if (y + BALL_SIZE <= BRICKS_Y || y >= BRICKS_BOTTOM) {
        return false;
    }

    int8_t first_col = (x - FIELD_LEFT) / BRICK_CELL_WIDTH;
    int8_t last_col  = (x + BALL_SIZE - 1 - FIELD_LEFT) / BRICK_CELL_WIDTH;
    int8_t first_row = (y - BRICKS_Y) / BRICK_CELL_HEIGHT;
    int8_t last_row  = (y + BALL_SIZE - 1 - BRICKS_Y) / BRICK_CELL_HEIGHT;
    // Por encima de los ladrillos la división redondea hacia cero: se corrige a mano
    if (y < BRICKS_Y) {
        first_row = 0;
    }
    bool hit = false;

    for (int8_t row = first_row; row <= last_row && row < BRICK_ROWS; row++) {
        for (int8_t col = first_col; col <= last_col && col < BRICK_COLS; col++) {
            if (col < 0 || !(bricks[row] & (1 << col))) {
                continue;
            }
            bricks[row] &= ~(1 << col);
            bricks_left--;
            draw_brick(col, row, GAME_COLOR_BLACK);
            score += (uint32_t)brick_points[row] * level;
            hit = true;
        }
    }
    if (score > MAX_SCORE) {
        score = MAX_SCORE;
    }
    return hit;
}

// Si la bola cae sobre la raqueta, rebota con el ángulo de la zona donde pegó
static void hit_paddle(void) {
    int16_t x = ball_px();
    int16_t y = ball_py();

    if (ball_vy <= 0 || !rects_overlap(x, y, BALL_SIZE, BALL_SIZE, paddle_x, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT)) {
        return;
    }
    // Solo cuenta si venía desde arriba: si ya iba por el costado, la bola sigue de largo
    if (y + BALL_SIZE - (ball_vy >> FIXED_SHIFT) > PADDLE_Y + 1) {
        return;
    }
    int16_t zone = (x + BALL_SIZE / 2 - paddle_x) * PADDLE_ZONES / PADDLE_WIDTH;
    if (zone < 0) {
        zone = 0;
    } else if (zone >= PADDLE_ZONES) {
        zone = PADDLE_ZONES - 1;
    }
    ball_y = (int32_t)(PADDLE_Y - BALL_SIZE) << FIXED_SHIFT;
    set_direction(zone_dx[zone], zone_dy[zone]);
}

static void lose_life(void) {
    erase_ball();
    lives--;
    draw_hud_status();
    if (lives == 0) {
        finish_game();
        return;
    }
    serve();
}

static void next_level(void) {
    erase_ball();
    level++;
    draw_hud_status();
    fill_bricks();
    // Los ladrillos se dibujan de a una fila por paso; mientras tanto la bola espera en la raqueta
    serve();
}

// Avanza la bola un paso: primero en horizontal y después en vertical, así cada choque sabe de qué
// lado vino
static void step_ball(void) {
    uint32_t score_before = score;

    ball_x += ball_vx;
    if (ball_px() < FIELD_LEFT) {
        ball_x  = (int32_t)FIELD_LEFT << FIXED_SHIFT;
        ball_vx = -ball_vx;
    } else if (ball_px() + BALL_SIZE > FIELD_RIGHT) {
        ball_x  = (int32_t)(FIELD_RIGHT - BALL_SIZE) << FIXED_SHIFT;
        ball_vx = -ball_vx;
    } else if (hit_bricks(ball_px(), ball_py())) {
        ball_x -= ball_vx;
        ball_vx = -ball_vx;
    }

    ball_y += ball_vy;
    if (ball_py() < FIELD_TOP) {
        ball_y  = (int32_t)FIELD_TOP << FIXED_SHIFT;
        ball_vy = -ball_vy;
    } else if (hit_bricks(ball_px(), ball_py())) {
        ball_y -= ball_vy;
        ball_vy = -ball_vy;
    } else {
        hit_paddle();
    }

    if (score != score_before) {
        draw_hud_score();
    }
    if (ball_py() >= GAME_SCREEN_HEIGHT) {
        lose_life();
        return;
    }
    redraw_ball();
    if (bricks_left == 0) {
        next_level();
    }
}

static void move_paddle(int16_t delta) {
    int16_t new_x = paddle_x + delta;
    if (new_x < FIELD_LEFT) {
        new_x = FIELD_LEFT;
    } else if (new_x > FIELD_RIGHT - PADDLE_WIDTH) {
        new_x = FIELD_RIGHT - PADDLE_WIDTH;
    }
    if (new_x == paddle_x) {
        return;
    }

    // Solo se borra la franja que la raqueta deja libre
    if (new_x > paddle_x) {
        int16_t freed = new_x - paddle_x < PADDLE_WIDTH ? new_x - paddle_x : PADDLE_WIDTH;
        game_host_fill_rect(paddle_x, PADDLE_Y, freed, PADDLE_HEIGHT, GAME_COLOR_BLACK);
    } else {
        int16_t freed = paddle_x - new_x < PADDLE_WIDTH ? paddle_x - new_x : PADDLE_WIDTH;
        game_host_fill_rect(paddle_x + PADDLE_WIDTH - freed, PADDLE_Y, freed, PADDLE_HEIGHT, GAME_COLOR_BLACK);
    }
    paddle_x = new_x;
    draw_paddle();

    if (state == STATE_SERVING) {
        place_ball_on_paddle();
        redraw_ball();
    } else if (ball_visible && rects_overlap(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, paddle_x, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT)) {
        // La raqueta pasó por encima de la bola: se repinta la bola para que no desaparezca
        game_host_fill_rect(drawn_x, drawn_y, BALL_SIZE, BALL_SIZE, ball_color);
    }
}

static void launch(uint32_t now_ms) {
    // Sale hacia un lado al azar con 30 grados respecto a la vertical
    uint8_t zone = random_next() & 1 ? 2 : PADDLE_ZONES - 3;
    set_direction(zone_dx[zone], zone_dy[zone]);
    state      = STATE_PLAYING;
    step_timer = now_ms;
}

static void start_game(uint32_t now_ms) {
    score    = 0;
    lives    = LIVES_START;
    level    = 1;
    paddle_x = PADDLE_START_X;
    fill_bricks();

    draw_hud_score();
    draw_hud_status();
    // El recuadro tapó parte de la cancha: se borra y se redibuja completa
    game_host_fill_rect(FIELD_LEFT, FIELD_TOP, FIELD_RIGHT - FIELD_LEFT, GAME_SCREEN_HEIGHT - FIELD_TOP, GAME_COLOR_BLACK);
    for (; rows_drawn < BRICK_ROWS; rows_drawn++) {
        draw_brick_row(rows_drawn);
    }
    draw_paddle();
    ball_visible = false;
    serve();
    step_timer = now_ms;
}

void game_breakout_init(uint32_t seed, uint32_t now_ms) {
    rng_state    = seed != 0 ? seed : 1;
    record       = game_host_record_load(GAME_ID_BREAKOUT);
    state        = STATE_READY;
    score        = 0;
    lives        = LIVES_START;
    level        = 1;
    paddle_x     = PADDLE_START_X;
    ball_visible = false;
    step_timer   = now_ms;
    fill_bricks();

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    draw_hud_score();
    draw_hud_status();
    draw_walls();
    for (; rows_drawn < BRICK_ROWS; rows_drawn++) {
        draw_brick_row(rows_drawn);
    }
    draw_paddle();

    char text[16];
    snprintf(text, sizeof(text), "REC %06lu", (unsigned long)record);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, "BREAKOUT", text, "CLIC: JUGAR");
}

void game_breakout_input(game_input_t input, uint32_t now_ms) {
    if (state == STATE_READY || state == STATE_OVER) {
        if (input == GAME_INPUT_ACTION) {
            start_game(now_ms);
        }
        return;
    }

    switch (input) {
        case GAME_INPUT_LEFT:
            move_paddle(-PADDLE_STEP);
            break;
        case GAME_INPUT_RIGHT:
            move_paddle(PADDLE_STEP);
            break;
        case GAME_INPUT_ACTION:
            // No se saca mientras se dibujan los ladrillos del nivel nuevo
            if (state == STATE_SERVING && rows_drawn == BRICK_ROWS) {
                launch(now_ms);
            }
            break;
        default:
            break;
    }
}

void game_breakout_tick(uint32_t now_ms) {
    if (state != STATE_SERVING && state != STATE_PLAYING) {
        return;
    }
    if (now_ms - step_timer < STEP_MS) {
        return;
    }
    step_timer += STEP_MS;
    if (now_ms - step_timer >= MAX_STEP_LAG * STEP_MS) {
        step_timer = now_ms;
    }

    if (rows_drawn < BRICK_ROWS) {
        draw_brick_row(rows_drawn++);
    } else if (state == STATE_PLAYING) {
        step_ball();
    }
}
