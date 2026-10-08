#pragma once

#include "game_host.h"

// Carritos: esquivar el tráfico cambiando de carril, al estilo de las consolas "Brick Game".
// Girar la rueda (GAME_INPUT_LEFT / GAME_INPUT_RIGHT) cambia de carril; el clic
// (GAME_INPUT_ACTION) empieza la partida.

// Prepara el juego y dibuja la pantalla de inicio. seed alimenta el azar del tráfico.
void game_car_init(uint32_t seed, uint32_t now_ms);

void game_car_input(game_input_t input, uint32_t now_ms);

// Avanza el juego; hay que llamarlo seguido con el reloj en milisegundos
void game_car_tick(uint32_t now_ms);
