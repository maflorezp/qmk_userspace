#pragma once

#include "game_host.h"

// Fuente de mapa de bits de 5x7 para los juegos: dígitos, mayúsculas y unos pocos signos.
// Se dibuja con rectángulos, así no depende de las fuentes de Quantum Painter.

#define GAME_FONT_GLYPH_WIDTH 5
#define GAME_FONT_GLYPH_HEIGHT 7
// Ancho que ocupa cada carácter contando la separación con el siguiente
#define GAME_FONT_ADVANCE 6

// Ancho y alto en píxeles de un texto con la escala dada
int16_t game_font_text_width(const char *text, uint8_t scale);
int16_t game_font_text_height(uint8_t scale);

// Dibuja el texto con su fondo; (x, y) es la esquina superior izquierda
void game_font_draw_text(int16_t x, int16_t y, const char *text, uint8_t scale, game_color_t color, game_color_t background);
