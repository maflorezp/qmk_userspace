#include "game_font.h"

#include <string.h>

// El carácter va en Latin-1 (ISO-8859-1): ASCII más las vocales con tilde, la ñ, la ü y los
// signos de apertura del español, cada uno en un solo byte
typedef struct {
    uint8_t character;
    uint8_t rows[GAME_FONT_GLYPH_HEIGHT];
} glyph_t;

// Cada fila usa los 5 bits bajos; el bit 4 es la columna de la izquierda
static const glyph_t glyphs[] = {
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
    {':', {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000}},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
    {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100}},
    {'/', {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000}},
    {'+', {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}},
    {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100}},
    {',', {0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b00100, 0b01000}},
    {'?', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100}},
    {'%', {0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011}},
    {'_', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111}},
    {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
    {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
    {';', {0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b00100, 0b01000}},
    {'"', {0b01010, 0b01010, 0b01010, 0b00000, 0b00000, 0b00000, 0b00000}},
    {'\'', {0b00100, 0b00100, 0b01000, 0b00000, 0b00000, 0b00000, 0b00000}},
    {'@', {0b01110, 0b10001, 0b10111, 0b10101, 0b10111, 0b10000, 0b01110}},
    {'#', {0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010}},
    {'$', {0b00100, 0b01111, 0b10100, 0b01110, 0b00101, 0b11110, 0b00100}},
    {'&', {0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101}},
    {'=', {0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000}},
    {'*', {0b00000, 0b00100, 0b10101, 0b01110, 0b10101, 0b00100, 0b00000}},
    {'<', {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010}},
    {'>', {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000}},
    {'[', {0b01110, 0b01000, 0b01000, 0b01000, 0b01000, 0b01000, 0b01110}},
    {']', {0b01110, 0b00010, 0b00010, 0b00010, 0b00010, 0b00010, 0b01110}},
    {0xBF, {0b00100, 0b00000, 0b00100, 0b01000, 0b10000, 0b10001, 0b01110}}, // ¿
    {0xA1, {0b00100, 0b00000, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}}, // ¡
    // Minúsculas: el cuerpo ocupa las filas 2 a 6 y los trazos altos suben hasta la fila 0. Las
    // que tienen cola (g, j, p, q, y) suben el cuerpo a las filas 1 a 4 para que la cola quepa.
    {'a', {0b00000, 0b00000, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111}},
    {'b', {0b10000, 0b10000, 0b10110, 0b11001, 0b10001, 0b10001, 0b11110}},
    {'c', {0b00000, 0b00000, 0b01110, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'d', {0b00001, 0b00001, 0b01101, 0b10011, 0b10001, 0b10001, 0b01111}},
    {'e', {0b00000, 0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110}},
    {'f', {0b00110, 0b01001, 0b01000, 0b11100, 0b01000, 0b01000, 0b01000}},
    {'g', {0b00000, 0b01111, 0b10001, 0b10001, 0b01111, 0b00001, 0b01110}},
    {'h', {0b10000, 0b10000, 0b10110, 0b11001, 0b10001, 0b10001, 0b10001}},
    {'i', {0b00100, 0b00000, 0b01100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'j', {0b00010, 0b00000, 0b00110, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'k', {0b10000, 0b10000, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010}},
    {'l', {0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'m', {0b00000, 0b00000, 0b11010, 0b10101, 0b10101, 0b10001, 0b10001}},
    {'n', {0b00000, 0b00000, 0b10110, 0b11001, 0b10001, 0b10001, 0b10001}},
    {'o', {0b00000, 0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'p', {0b00000, 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000}},
    {'q', {0b00000, 0b01111, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001}},
    {'r', {0b00000, 0b00000, 0b10110, 0b11001, 0b10000, 0b10000, 0b10000}},
    {'s', {0b00000, 0b00000, 0b01111, 0b10000, 0b01110, 0b00001, 0b11110}},
    {'t', {0b01000, 0b01000, 0b11100, 0b01000, 0b01000, 0b01001, 0b00110}},
    {'u', {0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101}},
    {'v', {0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'w', {0b00000, 0b00000, 0b10001, 0b10001, 0b10101, 0b10101, 0b01010}},
    {'x', {0b00000, 0b00000, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001}},
    {'y', {0b00000, 0b10001, 0b10001, 0b10001, 0b01111, 0b00001, 0b01110}},
    {'z', {0b00000, 0b00000, 0b11111, 0b00010, 0b00100, 0b01000, 0b11111}},
    // Letras del español: la tilde ocupa las filas 0 y 1, y la letra las filas 2 a 6. Las
    // mayúsculas se encogen a 5 filas para que la tilde quepa sin pegarse; por eso la ó y la í
    // bajan su cuerpo a 4 filas, si no serían iguales a la Ó y la Í.
    {0xE1, {0b00010, 0b00100, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111}}, // á
    {0xE9, {0b00010, 0b00100, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110}}, // é
    {0xED, {0b00010, 0b00100, 0b00000, 0b01100, 0b00100, 0b00100, 0b01110}}, // í
    {0xF3, {0b00010, 0b00100, 0b00000, 0b01110, 0b10001, 0b10001, 0b01110}}, // ó
    {0xFA, {0b00010, 0b00100, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101}}, // ú
    {0xFC, {0b01010, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101}}, // ü
    {0xF1, {0b01101, 0b10010, 0b00000, 0b10110, 0b11001, 0b10001, 0b10001}}, // ñ
    {0xC1, {0b00010, 0b00100, 0b01110, 0b10001, 0b11111, 0b10001, 0b10001}}, // Á
    {0xC9, {0b00010, 0b00100, 0b11111, 0b10000, 0b11110, 0b10000, 0b11111}}, // É
    {0xCD, {0b00010, 0b00100, 0b01110, 0b00100, 0b00100, 0b00100, 0b01110}}, // Í
    {0xD3, {0b00010, 0b00100, 0b01110, 0b10001, 0b10001, 0b10001, 0b01110}}, // Ó
    {0xDA, {0b00010, 0b00100, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}, // Ú
    {0xDC, {0b01010, 0b00000, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}}, // Ü
    {0xD1, {0b01101, 0b10010, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001}}, // Ñ
};

// Un carácter sin dibujo (el espacio, por ejemplo) queda en blanco
static const uint8_t *glyph_rows(uint8_t character) {
    for (uint8_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].character == character) {
            return glyphs[i].rows;
        }
    }
    return NULL;
}

// Pinta solo los píxeles de la letra de un carácter; el fondo queda como estaba
static void draw_glyph_rows(int16_t x, int16_t y, const uint8_t *rows, uint8_t scale, game_color_t color) {
    for (uint8_t row = 0; row < GAME_FONT_GLYPH_HEIGHT; row++) {
        // Los píxeles seguidos de una fila salen en un solo rectángulo
        uint8_t column = 0;
        while (column < GAME_FONT_GLYPH_WIDTH) {
            if (!(rows[row] & (1 << (GAME_FONT_GLYPH_WIDTH - 1 - column)))) {
                column++;
                continue;
            }
            uint8_t run_start = column;
            while (column < GAME_FONT_GLYPH_WIDTH && (rows[row] & (1 << (GAME_FONT_GLYPH_WIDTH - 1 - column)))) {
                column++;
            }
            game_host_fill_rect(x + run_start * scale, y + row * scale, (column - run_start) * scale, scale, color);
        }
    }
}

int16_t game_font_text_width(const char *text, uint8_t scale) {
    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }
    // El último carácter no lleva separación detrás
    return (int16_t)((length * GAME_FONT_ADVANCE - (GAME_FONT_ADVANCE - GAME_FONT_GLYPH_WIDTH)) * scale);
}

int16_t game_font_text_height(uint8_t scale) {
    return GAME_FONT_GLYPH_HEIGHT * scale;
}

void game_font_draw_text(int16_t x, int16_t y, const char *text, uint8_t scale, game_color_t color, game_color_t background) {
    game_host_fill_rect(x, y, game_font_text_width(text, scale), game_font_text_height(scale), background);

    for (; *text != '\0'; text++, x += GAME_FONT_ADVANCE * scale) {
        const uint8_t *rows = glyph_rows((uint8_t)*text);
        if (rows != NULL) {
            draw_glyph_rows(x, y, rows, scale, color);
        }
    }
}

void game_font_draw_glyph(int16_t x, int16_t y, uint8_t character, uint8_t scale, game_color_t color) {
    const uint8_t *rows = glyph_rows(character);
    if (rows != NULL) {
        draw_glyph_rows(x, y, rows, scale, color);
    }
}

// Fila del dibujo que cae en la línea offset, contada desde arriba del carácter; fuera del
// carácter todo es fondo
static uint8_t row_bits_at(const uint8_t *rows, int16_t offset, uint8_t scale) {
    if (offset < 0 || offset >= GAME_FONT_GLYPH_HEIGHT * scale) {
        return 0;
    }
    return rows[offset / scale];
}

void game_font_move_glyph(int16_t x, int16_t from_y, int16_t to_y, uint8_t character, uint8_t scale, game_color_t color, game_color_t background) {
    const uint8_t *rows = glyph_rows(character);
    if (rows == NULL || from_y == to_y) {
        return;
    }

    int16_t height = GAME_FONT_GLYPH_HEIGHT * scale;
    int16_t top    = from_y < to_y ? from_y : to_y;
    int16_t bottom = (from_y > to_y ? from_y : to_y) + height;
    int16_t y      = top;

    while (y < bottom) {
        uint8_t old_bits = row_bits_at(rows, y - from_y, scale);
        uint8_t new_bits = row_bits_at(rows, y - to_y, scale);
        // Las líneas seguidas que tenían y tendrán lo mismo se pintan en una sola franja
        int16_t band_end = y + 1;
        while (band_end < bottom && row_bits_at(rows, band_end - from_y, scale) == old_bits && row_bits_at(rows, band_end - to_y, scale) == new_bits) {
            band_end++;
        }

        // Solo se pintan las columnas que cambian: cada tramo seguido que pasa a letra, o a fondo,
        // sale en un rectángulo
        uint8_t changed = old_bits ^ new_bits;
        uint8_t column  = 0;
        while (column < GAME_FONT_GLYPH_WIDTH) {
            uint8_t mask = 1 << (GAME_FONT_GLYPH_WIDTH - 1 - column);
            if (!(changed & mask)) {
                column++;
                continue;
            }
            uint8_t to_letter = new_bits & mask;
            uint8_t run_start = column;
            while (column < GAME_FONT_GLYPH_WIDTH) {
                mask = 1 << (GAME_FONT_GLYPH_WIDTH - 1 - column);
                if (!(changed & mask) || (new_bits & mask) != (to_letter ? mask : 0)) {
                    break;
                }
                column++;
            }
            game_host_fill_rect(x + run_start * scale, y, (column - run_start) * scale, band_end - y, to_letter ? color : background);
        }
        y = band_end;
    }
}
