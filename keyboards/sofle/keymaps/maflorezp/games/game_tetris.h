#pragma once

#include "game_host.h"

// Tetris en un tablero de 10 x 20.
// Izquierda/derecha (o girar la rueda) mueven la pieza; arriba o el clic la giran; abajo la baja.

void game_tetris_init(uint32_t seed, uint32_t now_ms);
void game_tetris_input(game_input_t input, uint32_t now_ms);
void game_tetris_tick(uint32_t now_ms);
