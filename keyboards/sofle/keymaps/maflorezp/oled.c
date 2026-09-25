#include "maflorezp.h"
#include <stdio.h>
#include <string.h>

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    // Pantallas de 128x32 montadas en vertical
    return OLED_ROTATION_270;
}

static const char *layer_name(void) {
    switch (get_highest_layer(layer_state | default_layer_state)) {
        case _QWERTY:
            return "QWRT";
        case _LOWER:
            return "LOWER";
        case _RAISE:
            return "RAISE";
        case _NUMERIC:
            return "NUM  ";
        case _ADJUST:
            return "ADJ  ";
        case _RGB:
            return "RGB  ";
        default:
            return "???  ";
    }
}

// Pantalla vertical de 5 columnas x 16 líneas. Orden del bloque:
//   fecha D/M · hora H:M (los ":" parpadean) · en blanco · capa · CA NU · W CAS
//   · en blanco · W:xxx (vacía si la velocidad es 0) · REC (parpadea mientras se graba una macro) · USB
// y la versión (5 caracteres) en la última línea. Contra el quemado, una animación (culebrita
// o Pac-Man) recorre la pantalla comiéndose los píxeles (render_animation). Todo es
// configurable desde VIA (settings.c).

void display_apply_settings(void) {
    oled_set_brightness(((uint16_t)settings()->display.brightness_percent * 255) / 100);
}

// Alterna según los parpadeos por segundo configurados; sin parpadeo, siempre visible
static bool blink_visible(void) {
    uint8_t blinks = settings()->display.blinks_per_second;
    return blinks == 0 || (timer_read32() / (500 / blinks)) % 2 == 0;
}

static void render_status(void) {
    char line[8];

    oled_set_cursor(0, 0);

    if (clock_is_set()) {
        clock_time_t now = clock_now();
        snprintf(line, sizeof(line), "%02u/%02u", now.day, now.month);
        oled_write_ln(line, false);
        snprintf(line, sizeof(line), "%02u%c%02u", now.hour, blink_visible() ? ':' : ' ', now.minute);
        oled_write_ln(line, false);
    } else {
        oled_write_ln_P(PSTR("--/--"), false);
        oled_write_ln_P(PSTR("--:--"), false);
    }
    oled_write_ln_P(PSTR(""), false);

    oled_write_ln(layer_name(), false);

    // Posiciones fijas: cada indicador aparece solo mientras está activo
    // Posiciones fijas, sin separador para no dejar píxeles encendidos todo el tiempo
    led_t leds = host_keyboard_led_state();
    snprintf(line, sizeof(line), "%s %s", (leds.caps_lock || is_caps_word_on()) ? "CA" : "  ", leds.num_lock ? "NU" : "  ");
    oled_write_ln(line, false);

    uint8_t mods = get_mods() | get_oneshot_mods();
    line[0]      = (mods & MOD_MASK_GUI) ? 'W' : ' ';
    line[1]      = ' ';
    line[2]      = (mods & MOD_MASK_CTRL) ? 'C' : ' ';
    line[3]      = (mods & MOD_MASK_ALT) ? 'A' : ' ';
    line[4]      = (mods & MOD_MASK_SHIFT) ? 'S' : ' ';
    line[5]      = '\0';
    oled_write_ln(line, false);
    oled_write_ln_P(PSTR(""), false);

    // Velocidad de tipeo; en reposo (0) la línea queda apagada
    uint8_t wpm = get_current_wpm();
    if (wpm > 0) {
        snprintf(line, sizeof(line), "W:%3u", wpm);
        oled_write_ln(line, false);
    } else {
        oled_write_ln_P(PSTR(""), false);
    }

    oled_write_ln((is_recording_macro && blink_visible()) ? "REC" : "", false);
    oled_write_ln(is_keyboard_master() ? "USB" : "", false);

    // Versión en la última línea; invertida si la otra mitad no coincide o no responde
    snprintf(line, 6, "%s", firmware_version());
    bool warn = is_keyboard_master() && version_status() != VERSION_MATCH;
    oled_set_cursor(0, oled_max_lines() - 1);
    oled_write(line, warn);
}

// Animaciones contra el quemado: recorren la pantalla en zigzag por carriles (izquierda a
// derecha, bajan, derecha a izquierda...) y se "comen" lo que recorren: esos píxeles se
// apagan. Cada carril que termina queda apagado anim_pause_tenths y luego reaparece; al llegar
// abajo vuelven a empezar arriba. El recorrido es común; cada animación solo cambia el dibujo:
//   - culebrita: cabeza de 5x7 con un ojo y cuerpo de 3 px de grosor y 15 px de largo
//   - Pac-Man: sprite de 7x7 con la boca que se abre y se cierra
// Las dos van por carriles de 8 px: cada pasada se come una línea de texto completa.
#define SNAKE_LANE_PX 8
#define SNAKE_HEAD_LENGTH 5
#define SNAKE_HEAD_HEIGHT 7
#define SNAKE_BODY_WIDTH 3
#define SNAKE_BODY_LENGTH 15
#define PACMAN_LANE_PX 8
#define PACMAN_SIZE 7
#define PACMAN_CHOMP_STEPS 3 // pasos entre abrir y cerrar la boca

typedef struct {
    uint8_t x;
    uint8_t y;
} point_t;

// Estado del recorrido en un instante dado
typedef struct {
    uint8_t  width;
    uint16_t height;
    uint8_t  lane_px;
    uint32_t head;    // paso del recorrido en el que va la cabeza
    uint16_t lane;    // carril actual
    point_t  head_at; // posición de la cabeza (al centro del carril)
} path_t;

// Posición en pantalla del paso "position" del recorrido en zigzag (al centro del carril)
static point_t path_point(const path_t *path, uint32_t position) {
    uint16_t lane   = position / path->width;
    uint8_t  column = position % path->width;
    uint16_t center = lane * path->lane_px + path->lane_px / 2;
    return (point_t){
        .x = (lane % 2 == 0) ? column : path->width - 1 - column,
        .y = center < path->height ? center : path->height - 1,
    };
}

// Apaga las columnas [from, to] de las filas de píxeles [top, bottom)
static void clear_area(uint8_t from, uint8_t to, uint16_t top, uint16_t bottom) {
    for (uint16_t y = top; y < bottom; y++) {
        for (uint8_t x = from; x <= to; x++) {
            oled_write_pixel(x, y, false);
        }
    }
}

// Duración de una vuelta completa con carriles de lane_px
static uint32_t cycle_ms(uint8_t lane_px) {
    uint8_t  width  = oled_max_chars() * OLED_FONT_WIDTH;
    uint16_t height = oled_max_lines() * OLED_FONT_HEIGHT;
    uint16_t lanes  = (height + lane_px - 1) / lane_px;
    return (uint32_t)lanes * width * settings()->display.anim_step_10ms * 10UL;
}

// Apaga lo comido hasta el instante t (ms dentro de la vuelta) y devuelve dónde va la cabeza
static path_t eat_path(uint8_t lane_px, uint32_t t) {
    const display_settings_t *config = &settings()->display;
    path_t path = {
        .width   = oled_max_chars() * OLED_FONT_WIDTH,
        .height  = oled_max_lines() * OLED_FONT_HEIGHT,
        .lane_px = lane_px,
    };
    uint32_t step_ms  = config->anim_step_10ms * 10UL;
    uint32_t lane_ms  = path.width * step_ms;
    uint32_t pause_ms = config->anim_pause_tenths * 100UL;
    path.head         = t / step_ms;
    path.lane         = path.head / path.width;
    path.head_at      = path_point(&path, path.head);

    // Carriles ya terminados que siguen en su pausa: apagados completos
    for (uint16_t done = path.lane; done > 0; done--) {
        if (t - done * lane_ms >= pause_ms) {
            break;
        }
        uint16_t top = (done - 1) * lane_px;
        clear_area(0, path.width - 1, top, MIN(top + lane_px, path.height));
    }

    // Carril actual: comido desde su inicio hasta la cabeza
    uint16_t top      = path.lane * lane_px;
    bool     rightway = path.lane % 2 == 0;
    clear_area(rightway ? 0 : path.head_at.x, rightway ? path.head_at.x : path.width - 1, top, MIN(top + lane_px, path.height));
    return path;
}

// Cabeza de 5x7 mirando a la derecha (bit 4 = columna trasera): esquinas redondeadas y un ojo
static const uint8_t snake_head[SNAKE_HEAD_HEIGHT] = {0x0E, 0x1F, 0x1D, 0x1F, 0x1F, 0x1F, 0x0E};

// Enciende un cuadro de size x size centrado en (cx, cy), recortado a la pantalla
static void fill_square(const path_t *path, int16_t cx, int16_t cy, uint8_t size) {
    for (int8_t dy = -(size / 2); dy < size - size / 2; dy++) {
        for (int8_t dx = -(size / 2); dx < size - size / 2; dx++) {
            int16_t x = cx + dx;
            int16_t y = cy + dy;
            if (x >= 0 && x < path->width && y >= 0 && y < path->height) {
                oled_write_pixel(x, y, true);
            }
        }
    }
}

static void draw_snake(uint32_t t) {
    path_t path  = eat_path(SNAKE_LANE_PX, t);
    bool   right = path.lane % 2 == 0;

    // Cuerpo: 3 px de grosor detrás de la cabeza, también en el tramo vertical al doblar
    uint8_t first    = SNAKE_HEAD_LENGTH / 2 + 1;
    point_t previous = path_point(&path, path.head >= first - 1 ? path.head - (first - 1) : 0);
    for (uint8_t k = first; k < first + SNAKE_BODY_LENGTH && k <= path.head; k++) {
        point_t body = path_point(&path, path.head - k);
        for (uint16_t y = MIN(body.y, previous.y); y <= MAX(body.y, previous.y); y++) {
            fill_square(&path, body.x, y, SNAKE_BODY_WIDTH);
        }
        previous = body;
    }

    // Cabeza: se dibuja el cuadro completo (lo que no es cabeza queda apagado)
    for (uint8_t row = 0; row < SNAKE_HEAD_HEIGHT; row++) {
        for (uint8_t col = 0; col < SNAKE_HEAD_LENGTH; col++) {
            int16_t x = path.head_at.x - SNAKE_HEAD_LENGTH / 2 + col;
            int16_t y = path.head_at.y - SNAKE_HEAD_HEIGHT / 2 + row;
            if (x < 0 || x >= path.width || y < 0 || y >= path.height) {
                continue;
            }
            uint8_t bit = right ? (SNAKE_HEAD_LENGTH - 1 - col) : col;
            oled_write_pixel(x, y, (snake_head[row] >> bit) & 1);
        }
    }
}

// Sprites de 7x7 mirando a la derecha (bit 6 = columna izquierda)
static const uint8_t pacman_open[PACMAN_SIZE]   = {0x1C, 0x3E, 0x78, 0x70, 0x78, 0x3E, 0x1C};
static const uint8_t pacman_closed[PACMAN_SIZE] = {0x1C, 0x3E, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C};

static void draw_pacman(uint32_t t) {
    path_t         path   = eat_path(PACMAN_LANE_PX, t);
    const uint8_t *sprite = (path.head / PACMAN_CHOMP_STEPS) % 2 ? pacman_open : pacman_closed;
    bool           right  = path.lane % 2 == 0;

    // Se dibuja el cuadro completo: lo que no es Pac-Man queda apagado (la boca se ve vacía)
    for (uint8_t row = 0; row < PACMAN_SIZE; row++) {
        for (uint8_t col = 0; col < PACMAN_SIZE; col++) {
            int16_t x = path.head_at.x - PACMAN_SIZE / 2 + col;
            int16_t y = path.head_at.y - PACMAN_SIZE / 2 + row;
            if (x < 0 || x >= path.width || y < 0 || y >= path.height) {
                continue;
            }
            uint8_t bit = right ? (PACMAN_SIZE - 1 - col) : col;
            oled_write_pixel(x, y, (sprite[row] >> bit) & 1);
        }
    }
}

static void render_animation(void) {
    const display_settings_t *config = &settings()->display;
    if (config->animation == ANIMATION_NONE || config->anim_step_10ms == 0) {
        return;
    }
    uint32_t now = timer_read32();
    switch (config->animation) {
        case ANIMATION_SNAKE:
            draw_snake(now % cycle_ms(SNAKE_LANE_PX));
            break;
        case ANIMATION_PACMAN:
            draw_pacman(now % cycle_ms(PACMAN_LANE_PX));
            break;
        default: {
            // Alternar: una vuelta completa de la culebrita y luego una de Pac-Man
            uint32_t snake_ms = cycle_ms(SNAKE_LANE_PX);
            uint32_t t        = now % (snake_ms + cycle_ms(PACMAN_LANE_PX));
            if (t < snake_ms) {
                draw_snake(t);
            } else {
                draw_pacman(t - snake_ms);
            }
            break;
        }
    }
}

// Texto temporal sobre la pantalla (valores que se ajustan desde VIA)
#define OVERLAY_DURATION_MS 2000

static char     overlay[24]   = "";
static uint32_t overlay_timer = 0;

void display_show_overlay(const char *text) {
    strncpy(overlay, text, sizeof(overlay) - 1);
    overlay[sizeof(overlay) - 1] = '\0';
    overlay_timer                = timer_read32();
}

bool oled_task_user(void) {
#ifdef OLED_TEST_ENABLE
    // Diagnóstico: todos los píxeles encendidos, sin apagado automático
    static const char all_on[OLED_MATRIX_SIZE] = {[0 ... OLED_MATRIX_SIZE - 1] = (char)0xFF};
    oled_write_raw(all_on, sizeof(all_on));
    return false;
#endif
    // El valor que se está ajustando en VIA y los mensajes por HID se muestran aunque la
    // pantalla esté apagada por inactividad (lo que se dibuja la vuelve a encender)
    if (overlay[0] != '\0' && timer_elapsed32(overlay_timer) < OVERLAY_DURATION_MS) {
        oled_clear();
        oled_write(overlay, false);
        return false;
    }
    const char *message = raw_hid_message();
    if (message[0] != '\0') {
        oled_clear();
        oled_write(message, false);
        return false;
    }

    // Apagado propio: cuenta el tiempo sin teclear (con SPLIT_ACTIVITY_ENABLE, en las dos mitades).
    // Al volver a teclear, lo que se dibuja marca la pantalla como sucia y QMK la enciende sola.
    uint8_t timeout = settings()->display.timeout_seconds;
    if (timeout > 0 && last_input_activity_elapsed() > timeout * 1000UL) {
        if (is_oled_on()) {
            oled_off();
        }
        return false;
    }

    oled_clear();
#ifdef PIN_SCAN_ENABLE
    oled_write(pin_scan_report(), false);
    return false;
#endif
    render_status();
    render_animation();
    return false;
}
