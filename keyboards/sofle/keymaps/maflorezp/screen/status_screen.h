#pragma once

#include "game_host.h"

// Pantalla de estado de la pantalla a color (172x320, en vertical). Como los juegos, solo dibuja
// con game_host.h: el teclado le pasa el estado leído de QMK y el emulador del PC, uno inventado.
//
// De arriba hacia abajo: hora grande y fecha, capa (bloque con su color y nombre), bloqueos y
// modificadores (siempre dibujados, atenuados cuando están inactivos), REC y velocidad de
// escritura, mensaje de hasta 2 líneas, y USB con la versión. Cada dato tiene posición fija y solo
// se redibuja la zona cuyo dato cambió.

#define STATUS_MESSAGE_SIZE 32
#define STATUS_VERSION_SIZE 24

// Bits de status_state_t.mods
#define STATUS_MOD_GUI 0x01
#define STATUS_MOD_CTRL 0x02
#define STATUS_MOD_ALT 0x04
#define STATUS_MOD_SHIFT 0x08

typedef struct {
    bool    clock_set;
    uint8_t hour;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint8_t weekday; // 0 = domingo
    bool    colon_visible;

    const char  *layer_name;
    game_color_t layer_color;

    bool    caps_lock;
    bool    num_lock;
    uint8_t mods; // STATUS_MOD_*

    bool    recording;   // grabando una macro
    bool    rec_visible; // fase del parpadeo de REC
    uint8_t wpm;

    char message[STATUS_MESSAGE_SIZE]; // vacío: sin mensaje

    bool usb;
    char version[STATUS_VERSION_SIZE];
    bool version_warning; // la otra mitad no coincide o no responde
} status_state_t;

// La próxima llamada a status_screen_draw() dibuja la pantalla completa
void status_screen_reset(void);

// Dibuja lo que cambió desde la llamada anterior
void status_screen_draw(const status_state_t *state);

// Día de la semana (0 = domingo) de una fecha del calendario gregoriano
uint8_t status_weekday(uint16_t year, uint8_t month, uint8_t day);
