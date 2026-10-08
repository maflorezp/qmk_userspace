#pragma once

#include "game_host.h"

// Modo juego: un menú para elegir y el juego en curso. Quien lo aloja (el teclado o el emulador)
// solo habla con este archivo.

// Abre el menú. seed alimenta el azar de los juegos.
void game_system_init(uint32_t seed, uint32_t now_ms);

// Entra directo a un juego, sin pasar por el menú
void game_system_start(game_id_t game, uint32_t now_ms);

// Devuelve false cuando se pide salir del modo juego (GAME_INPUT_BACK estando en el menú)
bool game_system_input(game_input_t input, uint32_t now_ms);

void game_system_tick(uint32_t now_ms);
