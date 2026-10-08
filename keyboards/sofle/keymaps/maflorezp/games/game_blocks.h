#pragma once

#include "game_host.h"

// Tablero de cuadros al estilo de las consolas "Brick Game", compartido por los juegos de cuadros
// (carritos, Tetris, Breakout). El juego llena el tablero en memoria y game_blocks_flush() dibuja
// solo los cuadros que cambiaron desde la última vez: enviar la pantalla completa por SPI en cada
// paso sería demasiado lento.

#define GAME_BLOCKS_MAX_COLS 12
#define GAME_BLOCKS_MAX_ROWS 22
// Valor de un cuadro apagado; los demás valores son índices de la paleta
#define GAME_BLOCKS_EMPTY 0

typedef struct {
    int16_t             origin_x;
    int16_t             origin_y;
    uint8_t             cols;
    uint8_t             rows;
    uint8_t             cell_size;
    // palette[0] es el color tenue de los cuadros apagados
    const game_color_t *palette;
} game_blocks_config_t;

void game_blocks_init(const game_blocks_config_t *config);

// Apaga todos los cuadros en memoria (no dibuja)
void game_blocks_clear(void);

// Fija un cuadro en memoria; fuera del tablero se ignora
void game_blocks_set(int8_t col, int8_t row, uint8_t value);

// Dibuja los cuadros que cambiaron
void game_blocks_flush(void);

// Marca todo el tablero como pendiente: el próximo flush lo dibuja completo
void game_blocks_invalidate(void);
