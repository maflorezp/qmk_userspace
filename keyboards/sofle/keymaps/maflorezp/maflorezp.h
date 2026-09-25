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
void        hid_protocol_task(void);

// --- Reloj (clock.c) ---
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} clock_time_t;

bool         clock_is_set(void);
void         clock_set(const clock_time_t *time);
clock_time_t clock_now(void);

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

// --- Configuración editable desde VIA (settings.c) ---
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

#define RGB_LIMIT_MIN_PERCENT 10
#define DISPLAY_MAX_BLINKS 5

typedef enum {
    ANIMATION_NONE,
    ANIMATION_SNAKE,
    ANIMATION_PACMAN,
    ANIMATION_ALTERNATE, // una vuelta completa de cada una
    ANIMATION_COUNT,
} display_animation_t;

typedef struct {
    uint8_t timeout_seconds;    // se apaga tras este tiempo sin teclear; 0 = nunca
    uint8_t brightness_percent; // brillo de la pantalla
    uint8_t animation;          // animación contra el quemado (display_animation_t)
    uint8_t anim_step_10ms;     // tiempo por píxel avanzado (x 10 ms); 0 = detenida
    uint8_t anim_pause_tenths;  // cada carril terminado queda apagado este tiempo y luego reaparece (x 100 ms)
    uint8_t blinks_per_second;  // parpadeo de los ":" de la hora y de REC; 0 = sin parpadeo
} display_settings_t;

typedef struct {
    layer_color_t      colors[LAYER_COLOR_COUNT];
    uint8_t            rgb_limit_percent; // techo de brillo de la tira
    uint8_t            rgb_timeout_seconds; // la tira se apaga tras este tiempo sin teclear; 0 = nunca
    display_settings_t display;
} user_settings_t;

const user_settings_t *settings(void);
void                   settings_update(const user_settings_t *updated);
void                   settings_save(void);
void                   settings_init(void);
void                   settings_task(void);
uint8_t                settings_schema(void);

// --- Tira LED (rgb.c) y pantalla (oled.c): aplican la configuración ---
void rgb_init(void);
void rgb_apply_settings(void);
void rgb_task(void);
void layer_colors_toggle_manual(void);
void display_apply_settings(void);
// Muestra un texto corto por unos segundos (por ejemplo el valor que se ajusta en VIA)
void display_show_overlay(const char *text);

#ifdef PIN_SCAN_ENABLE
void        pin_scan_task(void);
const char *pin_scan_report(void);
#endif

#ifdef I2C_SCAN_ENABLE
void i2c_scan_task(void);
#endif
