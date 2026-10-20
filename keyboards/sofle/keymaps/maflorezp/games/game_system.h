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

// Un carácter tecleado, en Latin-1 (ISO-8859-1): ASCII imprimible más á é í ó ú ü ñ ¿ ¡ y sus
// mayúsculas. Devuelve true si el juego en curso recibe texto (la práctica de escritura); si no,
// quien aloja traduce la tecla a una entrada con game_system_input().
bool game_system_type(uint8_t character, uint32_t now_ms);

// true mientras el juego en curso recibe texto: quien aloja no debe usar las letras como flechas
bool game_system_wants_text(void);

void game_system_tick(uint32_t now_ms);
