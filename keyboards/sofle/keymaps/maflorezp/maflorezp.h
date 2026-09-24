#pragma once

#include QMK_KEYBOARD_H

enum layers {
    _QWERTY,
    _LOWER,
    _RAISE,
    _NUMERIC,
    _ADJUST,
    _RGB,
};

// En el rango QK_KB para que usevia.app las muestre con nombre (customKeycodes del JSON)
enum custom_keycodes {
    CK_RGB1 = QK_KB_0,
    CK_HAND,
};

extern bool is_recording_macro;

// Texto recibido por Raw HID para mostrarlo en la OLED (vacío si no hay mensaje vigente)
const char *raw_hid_message(void);

// --- Versión del firmware (version.c) ---
#define VERSION_STRING_SIZE 24

typedef enum {
    VERSION_UNKNOWN,  // la otra mitad no respondió
    VERSION_MATCH,    // las dos mitades tienen el mismo firmware
    VERSION_MISMATCH, // firmwares distintos: hay que flashear las dos
} version_status_t;

const char      *firmware_version(void);
const char      *other_half_version(void);
version_status_t version_status(void);
void             version_init(void);
void             version_task(void);

// --- Colores de la tira por capa (rgb.c) ---
// El orden define la prioridad de las capas de luz (el último gana) y los ids del menú de VIA
typedef enum {
    LAYER_COLOR_MANUAL,
    LAYER_COLOR_LOWER,
    LAYER_COLOR_RAISE,
    LAYER_COLOR_NUMERIC,
    LAYER_COLOR_ADJUST,
    LAYER_COLOR_RGB,
    LAYER_COLOR_CAPS,
    LAYER_COLOR_MACRO,
    LAYER_COLOR_COUNT,
} layer_color_item_t;

typedef struct {
    uint8_t enabled;
    uint8_t hue;
    uint8_t sat;
} layer_color_t;

const layer_color_t *layer_colors_get(layer_color_item_t item);
void                 layer_colors_set(layer_color_item_t item, const layer_color_t *color);
void                 layer_colors_save(void);
void                 layer_colors_toggle_manual(void);
uint8_t              brightness_limit_get_percent(void);
void                 brightness_limit_set_percent(uint8_t percent);
void                 layer_colors_init(void);
void                 layer_colors_task(void);

#ifdef PIN_SCAN_ENABLE
void        pin_scan_task(void);
const char *pin_scan_report(void);
#endif

#ifdef I2C_SCAN_ENABLE
void i2c_scan_task(void);
#endif
