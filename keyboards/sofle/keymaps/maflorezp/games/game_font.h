#pragma once

#include "game_host.h"

// Fuente de mapa de bits de 5x7 para los juegos: dígitos, mayúsculas, minúsculas, signos y las
// letras del español (vocales con tilde, ñ, ü, ¿ y ¡). Se dibuja con rectángulos, así no depende
// de las fuentes de Quantum Painter.
// Los textos van en Latin-1 (ISO-8859-1), un byte por carácter: en C se escriben las letras del
// español con \x (por ejemplo "MIN\xDASCULAS"), nunca en UTF-8.

#define GAME_FONT_GLYPH_WIDTH 5
#define GAME_FONT_GLYPH_HEIGHT 7
// Ancho que ocupa cada carácter contando la separación con el siguiente
#define GAME_FONT_ADVANCE 6

// Ancho y alto en píxeles de un texto con la escala dada
int16_t game_font_text_width(const char *text, uint8_t scale);
int16_t game_font_text_height(uint8_t scale);

// Dibuja el texto con su fondo; (x, y) es la esquina superior izquierda
void game_font_draw_text(int16_t x, int16_t y, const char *text, uint8_t scale, game_color_t color, game_color_t background);

// Pinta solo los píxeles de la letra de un carácter, sin tocar el fondo: sirve para dibujarlo sobre
// un fondo ya limpio, cambiarle el color o borrarlo pintándolo del color del fondo
void game_font_draw_glyph(int16_t x, int16_t y, uint8_t character, uint8_t scale, game_color_t color);

// Mueve en vertical un carácter que está pintado en from_y sobre el fondo dado y lo deja en to_y.
// Repinta solo los píxeles que cambian (de fondo a letra o de letra a fondo), sin borrar antes el
// recuadro: así el carácter no parpadea y por SPI viaja lo mínimo.
void game_font_move_glyph(int16_t x, int16_t from_y, int16_t to_y, uint8_t character, uint8_t scale, game_color_t color, game_color_t background);
