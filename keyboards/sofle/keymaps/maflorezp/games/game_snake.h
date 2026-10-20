#pragma once

#include "game_host.h"

// Culebrita: comer para crecer sin chocar con los bordes ni consigo misma. Las cuatro
// direcciones (GAME_INPUT_LEFT/RIGHT/UP/DOWN) son absolutas, también las de la rueda; se guardan
// hasta dos giros entre paso y paso. El clic (GAME_INPUT_ACTION) empieza la partida.

// Prepara el juego y dibuja la pantalla de inicio. seed alimenta el azar.
void game_snake_init(uint32_t seed, uint32_t now_ms);

void game_snake_input(game_input_t input, uint32_t now_ms);

// Avanza el juego; hay que llamarlo seguido con el reloj en milisegundos
void game_snake_tick(uint32_t now_ms);
