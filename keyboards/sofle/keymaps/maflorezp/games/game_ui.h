#pragma once

#include "game_host.h"

// Piezas de pantalla que comparten los juegos

// Tamaño del texto de los juegos y posición de las dos líneas de la franja superior
#define GAME_UI_TEXT_SCALE 2
#define GAME_UI_HUD_LINE_1_Y 4
#define GAME_UI_HUD_LINE_2_Y 22

// Recuadro con tres líneas centradas (la primera, resaltada), centrado en la altura center_y
void game_ui_draw_overlay(int16_t center_y, const char *line_1, const char *line_2, const char *line_3);
