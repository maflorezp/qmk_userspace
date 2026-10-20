#pragma once

#include "game_host.h"

// Práctica de escritura: caen letras y hay que teclear cada una antes de que toque el fondo.
// Cada tecla se lleva la letra igual que esté más abajo; una tecla sin letra baja la precisión y
// cada letra que llega al fondo cuesta una vida (hay 3). Con los aciertos las letras caen más
// rápido y salen más seguido.
// Antes de cada partida hay una pantalla de configuración: casillas para elegir qué conjuntos
// caen (minúsculas, mayúsculas, números, símbolos, tildes; al menos uno) y la velocidad inicial
// (1 a 9). Arriba/abajo mueven el cursor; izquierda/derecha (la rueda) también, salvo en la
// velocidad, donde la cambian. El clic (GAME_INPUT_ACTION) marca, sube la velocidad o empieza.
// Lo elegido se guarda con game_host_typing_preferences_save(). Las letras llegan por
// game_typing_type() solo mientras se juega (game_typing_wants_text()).

// Prepara el juego y dibuja la pantalla de configuración. seed alimenta el azar.
void game_typing_init(uint32_t seed, uint32_t now_ms);

void game_typing_input(game_input_t input, uint32_t now_ms);

// Un carácter tecleado en Latin-1 (ISO-8859-1): ASCII o una letra del español (á, Ñ, ü, ¿, ¡...).
// Se compara tal cual, distinguiendo mayúsculas de minúsculas.
void game_typing_type(uint8_t character, uint32_t now_ms);

// Avanza el juego; hay que llamarlo seguido con el reloj en milisegundos
void game_typing_tick(uint32_t now_ms);

// true solo mientras se juega: en la pantalla de configuración las teclas sirven para navegar
bool game_typing_wants_text(void);
