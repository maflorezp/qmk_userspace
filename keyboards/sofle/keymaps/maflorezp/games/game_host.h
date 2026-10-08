#pragma once

#include <stdbool.h>
#include <stdint.h>

// Lo que un juego necesita de quien lo aloja. Los juegos solo usan esta interfaz, así el mismo
// código corre en el teclado (Quantum Painter) y en el emulador del PC (tools/lcd_emulator).

#define GAME_SCREEN_WIDTH 172
#define GAME_SCREEN_HEIGHT 320

// Color en HSV de 8 bits por componente, igual que las primitivas de Quantum Painter
typedef struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} game_color_t;

#define GAME_COLOR_BLACK ((game_color_t){0, 0, 0})
#define GAME_COLOR_WHITE ((game_color_t){0, 0, 255})

typedef enum {
    GAME_INPUT_LEFT,
    GAME_INPUT_RIGHT,
    GAME_INPUT_UP,
    GAME_INPUT_DOWN,
    GAME_INPUT_ACTION,
    // Volver al menú; lo atiende game_system, no cada juego
    GAME_INPUT_BACK,
} game_input_t;

typedef enum {
    GAME_ID_CAR,
    GAME_ID_TETRIS,
    GAME_ID_COUNT,
} game_id_t;

// Rectángulo relleno; las coordenadas fuera de la pantalla se recortan
void game_host_fill_rect(int16_t x, int16_t y, int16_t width, int16_t height, game_color_t color);

// Récord guardado de cada juego
uint32_t game_host_record_load(game_id_t game);
void     game_host_record_save(game_id_t game, uint32_t record);
