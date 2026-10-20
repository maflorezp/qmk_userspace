#pragma once

#include "game_host.h"

// Breakout: romper los ladrillos con la bola. Girar la rueda o A/D mueven la raqueta
// (GAME_INPUT_LEFT / GAME_INPUT_RIGHT); el clic (GAME_INPUT_ACTION) empieza la partida y saca la
// bola. Tres vidas; al romper todos los ladrillos se pasa de nivel y la bola va un poco más rápido.

// Prepara el juego y dibuja la pantalla de inicio. seed alimenta el azar.
void game_breakout_init(uint32_t seed, uint32_t now_ms);

void game_breakout_input(game_input_t input, uint32_t now_ms);

// Avanza el juego; hay que llamarlo seguido con el reloj en milisegundos
void game_breakout_tick(uint32_t now_ms);
