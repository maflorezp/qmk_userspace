#include "game_blocks.h"

#include <string.h>

// Cada cuadro deja 1 px libre por lado y se dibuja como un marco con un núcleo relleno
#define CELL_MARGIN 1
#define CELL_BORDER 2
#define CELL_GAP 2
// Valor imposible en la paleta: obliga a redibujar el cuadro
#define CELL_UNKNOWN 0xFF

static game_blocks_config_t board;
static uint8_t              cells[GAME_BLOCKS_MAX_ROWS][GAME_BLOCKS_MAX_COLS];
static uint8_t              drawn[GAME_BLOCKS_MAX_ROWS][GAME_BLOCKS_MAX_COLS];

static void draw_cell(uint8_t col, uint8_t row, game_color_t color) {
    int16_t x     = board.origin_x + col * board.cell_size + CELL_MARGIN;
    int16_t y     = board.origin_y + row * board.cell_size + CELL_MARGIN;
    int16_t outer = board.cell_size - 2 * CELL_MARGIN;
    int16_t inner = outer - 2 * CELL_BORDER;
    int16_t core  = inner - 2 * CELL_GAP;

    game_host_fill_rect(x, y, outer, outer, color);
    game_host_fill_rect(x + CELL_BORDER, y + CELL_BORDER, inner, inner, GAME_COLOR_BLACK);
    game_host_fill_rect(x + CELL_BORDER + CELL_GAP, y + CELL_BORDER + CELL_GAP, core, core, color);
}

void game_blocks_init(const game_blocks_config_t *config) {
    board = *config;
    game_blocks_clear();
    game_blocks_invalidate();
}

void game_blocks_clear(void) {
    memset(cells, GAME_BLOCKS_EMPTY, sizeof(cells));
}

void game_blocks_set(int8_t col, int8_t row, uint8_t value) {
    if (col < 0 || row < 0 || col >= board.cols || row >= board.rows) {
        return;
    }
    cells[row][col] = value;
}

void game_blocks_flush(void) {
    for (uint8_t row = 0; row < board.rows; row++) {
        for (uint8_t col = 0; col < board.cols; col++) {
            if (cells[row][col] != drawn[row][col]) {
                draw_cell(col, row, board.palette[cells[row][col]]);
                drawn[row][col] = cells[row][col];
            }
        }
    }
}

void game_blocks_invalidate(void) {
    memset(drawn, CELL_UNKNOWN, sizeof(drawn));
}
