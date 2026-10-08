#include "game_tetris.h"

#include <stdio.h>
#include <string.h>

#include "game_blocks.h"
#include "game_font.h"
#include "game_ui.h"

#define FIELD_COLS 10
#define FIELD_ROWS 20
#define CELL_SIZE 14
#define FIELD_WIDTH (FIELD_COLS * CELL_SIZE)
#define FIELD_HEIGHT (FIELD_ROWS * CELL_SIZE)
#define FIELD_X ((GAME_SCREEN_WIDTH - FIELD_WIDTH) / 2)
// Franja superior con el puntaje, las líneas, el nivel y la pieza siguiente
#define HUD_HEIGHT (GAME_SCREEN_HEIGHT - FIELD_HEIGHT)
#define HUD_X 4

#define PIECE_COUNT 7
#define PIECE_SIZE 4
#define ROTATION_COUNT 4
#define SPAWN_COL ((FIELD_COLS - PIECE_SIZE) / 2)

// Vista de la pieza siguiente: cuadros pequeños en la esquina superior derecha. En su giro
// inicial todas las piezas caben en las dos primeras filas de su cuadrícula.
#define PREVIEW_CELL_SIZE 7
#define PREVIEW_ROWS 2
#define PREVIEW_X (GAME_SCREEN_WIDTH - HUD_X - PIECE_SIZE * PREVIEW_CELL_SIZE)
#define PREVIEW_Y GAME_UI_HUD_LINE_1_Y

#define LINES_PER_LEVEL 10
#define FALL_INTERVAL_START_MS 700
#define FALL_INTERVAL_PER_LEVEL_MS 60
#define FALL_INTERVAL_MIN_MS 80
// Tiempo que las líneas completas quedan resaltadas antes de desaparecer
#define CLEAR_FLASH_MS 180
#define MAX_SCORE 999999UL

enum palette_index {
    PALETTE_EMPTY = GAME_BLOCKS_EMPTY,
    // Las piezas usan los índices 1 a PIECE_COUNT
    PALETTE_FLASH = PIECE_COUNT + 1,
};

static const game_color_t palette[] = {
    [PALETTE_EMPTY] = {0, 0, 22},
    {128, 255, 255}, // I
    {170, 255, 255}, // J
    {21, 255, 255},  // L
    {43, 255, 255},  // O
    {85, 255, 255},  // S
    {200, 255, 255}, // T
    {0, 255, 255},   // Z
    [PALETTE_FLASH] = {0, 0, 255},
};

static const game_color_t hud_color = {0, 0, 200};

// Cada pieza en sus cuatro giros, como cuadrícula de 4x4: el bit 15 es la esquina superior
// izquierda y se avanza por filas
static const uint16_t piece_shapes[PIECE_COUNT][ROTATION_COUNT] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x8E00, 0x6440, 0x0E20, 0x44C0}, // J
    {0x2E00, 0x4460, 0x0E80, 0xC440}, // L
    {0x6600, 0x6600, 0x6600, 0x6600}, // O
    {0x6C00, 0x4620, 0x06C0, 0x8C40}, // S
    {0x4E00, 0x4640, 0x0E40, 0x4C40}, // T
    {0xC600, 0x2640, 0x0C60, 0x4C80}, // Z
};

// Puntos por líneas completadas de una vez (se multiplican por el nivel)
static const uint16_t line_scores[PIECE_SIZE + 1] = {0, 40, 100, 300, 1200};

// Al girar contra un muro o una pieza se prueba a correr la pieza estas columnas
static const int8_t rotation_kicks[] = {0, -1, 1, -2, 2};

typedef enum {
    STATE_READY,
    STATE_PLAYING,
    STATE_CLEARING,
    STATE_OVER,
} tetris_state_t;

static tetris_state_t state;
// Piezas ya fijadas: 0 vacío o el índice de color
static uint8_t  field[FIELD_ROWS][FIELD_COLS];
static uint8_t  piece;
static uint8_t  rotation;
static int8_t   piece_col;
static int8_t   piece_row;
static uint8_t  next_piece;
// Bolsa con las 7 piezas barajadas: ninguna tarda más de 12 turnos en volver a salir
static uint8_t  bag[PIECE_COUNT];
static uint8_t  bag_left;
static uint32_t score;
static uint32_t record;
static uint16_t lines;
static uint32_t fall_timer;
static uint32_t rng_state;

static uint32_t random_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static bool shape_has(uint8_t shape_piece, uint8_t shape_rotation, uint8_t row, uint8_t col) {
    return piece_shapes[shape_piece][shape_rotation] & (0x8000 >> (row * PIECE_SIZE + col));
}

static uint8_t level(void) {
    return 1 + lines / LINES_PER_LEVEL;
}

static uint16_t fall_interval(void) {
    int16_t interval = FALL_INTERVAL_START_MS - (level() - 1) * FALL_INTERVAL_PER_LEVEL_MS;
    return interval < FALL_INTERVAL_MIN_MS ? FALL_INTERVAL_MIN_MS : interval;
}

static uint8_t take_from_bag(void) {
    if (bag_left == 0) {
        for (uint8_t i = 0; i < PIECE_COUNT; i++) {
            bag[i] = i;
        }
        // Barajado de Fisher-Yates
        for (uint8_t i = PIECE_COUNT - 1; i > 0; i--) {
            uint8_t j    = random_next() % (i + 1);
            uint8_t swap = bag[i];
            bag[i]       = bag[j];
            bag[j]       = swap;
        }
        bag_left = PIECE_COUNT;
    }
    return bag[--bag_left];
}

static bool piece_fits(uint8_t test_rotation, int8_t test_col, int8_t test_row) {
    for (uint8_t row = 0; row < PIECE_SIZE; row++) {
        for (uint8_t col = 0; col < PIECE_SIZE; col++) {
            if (!shape_has(piece, test_rotation, row, col)) {
                continue;
            }
            int8_t field_col = test_col + col;
            int8_t field_row = test_row + row;
            if (field_col < 0 || field_col >= FIELD_COLS || field_row >= FIELD_ROWS) {
                return false;
            }
            // Por encima del borde superior no hay nada con qué chocar
            if (field_row >= 0 && field[field_row][field_col] != 0) {
                return false;
            }
        }
    }
    return true;
}

static bool row_is_full(uint8_t row) {
    for (uint8_t col = 0; col < FIELD_COLS; col++) {
        if (field[row][col] == 0) {
            return false;
        }
    }
    return true;
}

static void draw_hud(void) {
    char text[16];

    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_1_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
    snprintf(text, sizeof(text), "LIN %03u N%02u", lines, level());
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_2_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

static void draw_preview(void) {
    game_host_fill_rect(PREVIEW_X, PREVIEW_Y, PIECE_SIZE * PREVIEW_CELL_SIZE, PREVIEW_ROWS * PREVIEW_CELL_SIZE, GAME_COLOR_BLACK);
    for (uint8_t row = 0; row < PREVIEW_ROWS; row++) {
        for (uint8_t col = 0; col < PIECE_SIZE; col++) {
            if (shape_has(next_piece, 0, row, col)) {
                game_host_fill_rect(PREVIEW_X + col * PREVIEW_CELL_SIZE, PREVIEW_Y + row * PREVIEW_CELL_SIZE, PREVIEW_CELL_SIZE - 1, PREVIEW_CELL_SIZE - 1, palette[next_piece + 1]);
            }
        }
    }
}

static void render_field(void) {
    game_blocks_clear();

    for (uint8_t row = 0; row < FIELD_ROWS; row++) {
        bool flash = state == STATE_CLEARING && row_is_full(row);
        for (uint8_t col = 0; col < FIELD_COLS; col++) {
            if (field[row][col] != 0) {
                game_blocks_set(col, row, flash ? PALETTE_FLASH : field[row][col]);
            }
        }
    }
    if (state == STATE_PLAYING) {
        for (uint8_t row = 0; row < PIECE_SIZE; row++) {
            for (uint8_t col = 0; col < PIECE_SIZE; col++) {
                if (shape_has(piece, rotation, row, col)) {
                    game_blocks_set(piece_col + col, piece_row + row, piece + 1);
                }
            }
        }
    }

    game_blocks_flush();
}

static void finish_game(void) {
    char text[16];

    state = STATE_OVER;
    if (score > record) {
        record = score;
        game_host_record_save(GAME_ID_TETRIS, record);
    }
    render_field();
    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, "FIN", text, "CLIC: OTRA");
}

// Pone en juego la pieza siguiente; si no cabe, se acabó la partida
static void spawn_piece(uint32_t now_ms) {
    piece      = next_piece;
    next_piece = take_from_bag();
    rotation   = 0;
    piece_col  = SPAWN_COL;
    piece_row  = 0;
    fall_timer = now_ms;
    draw_preview();

    if (!piece_fits(rotation, piece_col, piece_row)) {
        finish_game();
    }
}

// Quita las líneas completas bajando lo que tienen encima
static void remove_full_rows(void) {
    int8_t write_row = FIELD_ROWS - 1;
    for (int8_t row = FIELD_ROWS - 1; row >= 0; row--) {
        if (row_is_full(row)) {
            continue;
        }
        if (write_row != row) {
            memcpy(field[write_row], field[row], FIELD_COLS);
        }
        write_row--;
    }
    for (; write_row >= 0; write_row--) {
        memset(field[write_row], 0, FIELD_COLS);
    }
}

// Fija la pieza en el tablero y cuenta las líneas que completó
static void lock_piece(uint32_t now_ms) {
    for (uint8_t row = 0; row < PIECE_SIZE; row++) {
        for (uint8_t col = 0; col < PIECE_SIZE; col++) {
            if (shape_has(piece, rotation, row, col) && piece_row + row >= 0) {
                field[piece_row + row][piece_col + col] = piece + 1;
            }
        }
    }

    uint8_t full_rows = 0;
    for (uint8_t row = 0; row < FIELD_ROWS; row++) {
        if (row_is_full(row)) {
            full_rows++;
        }
    }
    if (full_rows == 0) {
        spawn_piece(now_ms);
        return;
    }

    score += (uint32_t)line_scores[full_rows] * level();
    if (score > MAX_SCORE) {
        score = MAX_SCORE;
    }
    lines += full_rows;
    draw_hud();
    // Las líneas quedan resaltadas un instante; tick las quita y saca la pieza siguiente
    state      = STATE_CLEARING;
    fall_timer = now_ms;
}

static void move_down(uint32_t now_ms) {
    if (piece_fits(rotation, piece_col, piece_row + 1)) {
        piece_row++;
        fall_timer = now_ms;
    } else {
        lock_piece(now_ms);
    }
}

static void rotate(void) {
    uint8_t new_rotation = (rotation + 1) % ROTATION_COUNT;
    for (uint8_t i = 0; i < sizeof(rotation_kicks) / sizeof(rotation_kicks[0]); i++) {
        if (piece_fits(new_rotation, piece_col + rotation_kicks[i], piece_row)) {
            rotation = new_rotation;
            piece_col += rotation_kicks[i];
            return;
        }
    }
}

static void start_game(uint32_t now_ms) {
    memset(field, 0, sizeof(field));
    state      = STATE_PLAYING;
    score      = 0;
    lines      = 0;
    bag_left   = 0;
    next_piece = take_from_bag();

    draw_hud();
    // El recuadro tapó parte del tablero: se borra y se redibuja completo
    game_host_fill_rect(0, HUD_HEIGHT, GAME_SCREEN_WIDTH, FIELD_HEIGHT, GAME_COLOR_BLACK);
    game_blocks_invalidate();
    spawn_piece(now_ms);
    render_field();
}

void game_tetris_init(uint32_t seed, uint32_t now_ms) {
    static const game_blocks_config_t config = {
        .origin_x  = FIELD_X,
        .origin_y  = HUD_HEIGHT,
        .cols      = FIELD_COLS,
        .rows      = FIELD_ROWS,
        .cell_size = CELL_SIZE,
        .palette   = palette,
    };

    rng_state = seed != 0 ? seed : 1;
    record    = game_host_record_load(GAME_ID_TETRIS);
    memset(field, 0, sizeof(field));
    state      = STATE_READY;
    score      = 0;
    lines      = 0;
    fall_timer = now_ms;

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    game_blocks_init(&config);
    draw_hud();
    render_field();

    char text[16];
    snprintf(text, sizeof(text), "REC %06lu", (unsigned long)record);
    game_ui_draw_overlay(HUD_HEIGHT + FIELD_HEIGHT / 2, "TETRIS", text, "CLIC: JUGAR");
}

void game_tetris_input(game_input_t input, uint32_t now_ms) {
    if (state == STATE_READY || state == STATE_OVER) {
        if (input == GAME_INPUT_ACTION) {
            start_game(now_ms);
        }
        return;
    }
    if (state != STATE_PLAYING) {
        return;
    }

    switch (input) {
        case GAME_INPUT_LEFT:
            if (piece_fits(rotation, piece_col - 1, piece_row)) {
                piece_col--;
            }
            break;
        case GAME_INPUT_RIGHT:
            if (piece_fits(rotation, piece_col + 1, piece_row)) {
                piece_col++;
            }
            break;
        case GAME_INPUT_UP:
        case GAME_INPUT_ACTION:
            rotate();
            break;
        case GAME_INPUT_DOWN:
            move_down(now_ms);
            break;
        default:
            return;
    }
    // Si al bajar se acabó la partida, el recuadro ya está dibujado y no se toca el tablero
    if (state != STATE_OVER) {
        render_field();
    }
}

void game_tetris_tick(uint32_t now_ms) {
    if (state == STATE_PLAYING) {
        if (now_ms - fall_timer >= fall_interval()) {
            move_down(now_ms);
            if (state != STATE_OVER) {
                render_field();
            }
        }
    } else if (state == STATE_CLEARING && now_ms - fall_timer >= CLEAR_FLASH_MS) {
        remove_full_rows();
        state = STATE_PLAYING;
        spawn_piece(now_ms);
        if (state != STATE_OVER) {
            render_field();
        }
    }
}
