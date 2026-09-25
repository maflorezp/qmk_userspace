#include "maflorezp.h"
#include "via.h"
#include <stdio.h>

// Menú propio de usevia.app (definido en via/sofle_maflorezp.json), en el canal id_custom_channel.
//   value_id 1..16: por cada capa de luz, id impar = activo (1 byte), id par = color (tono, saturación)
//   value_id 17..24: valores de 1 byte (ver value_ids)
enum value_ids {
    VALUE_ID_RGB_LIMIT = LAYER_COLOR_COUNT * 2 + 1, // techo de brillo de la tira (%)
    VALUE_ID_DISPLAY_TIMEOUT,                       // apagado de la pantalla (segundos; 0 = nunca)
    VALUE_ID_DISPLAY_BRIGHTNESS,                    // brillo de la pantalla (%)
    VALUE_ID_DISPLAY_ANIM_STEP,                     // animación: tiempo por píxel (x 10 ms; 0 = detenida)
    VALUE_ID_DISPLAY_BLINKS,                        // parpadeos por segundo (0 = sin parpadeo)
    VALUE_ID_RGB_TIMEOUT,                           // apagado de la tira (segundos; 0 = nunca)
    VALUE_ID_DISPLAY_ANIMATION,                     // animación: ninguna, culebrita, Pac-Man o alternar
    VALUE_ID_DISPLAY_ANIM_PAUSE,                    // animación: tiempo que cada carril queda apagado (x 100 ms)
};

// Dirección del byte de configuración que corresponde a un value_id de 1 byte, o NULL
static uint8_t *byte_setting(user_settings_t *config, uint8_t value_id) {
    switch (value_id) {
        case VALUE_ID_RGB_LIMIT:
            return &config->rgb_limit_percent;
        case VALUE_ID_DISPLAY_TIMEOUT:
            return &config->display.timeout_seconds;
        case VALUE_ID_DISPLAY_BRIGHTNESS:
            return &config->display.brightness_percent;
        case VALUE_ID_DISPLAY_ANIM_STEP:
            return &config->display.anim_step_10ms;
        case VALUE_ID_DISPLAY_ANIMATION:
            return &config->display.animation;
        case VALUE_ID_DISPLAY_ANIM_PAUSE:
            return &config->display.anim_pause_tenths;
        case VALUE_ID_DISPLAY_BLINKS:
            return &config->display.blinks_per_second;
        case VALUE_ID_RGB_TIMEOUT:
            return &config->rgb_timeout_seconds;
        default:
            return NULL;
    }
}

#ifdef OLED_ENABLE
// Nombre corto (5 columnas) de cada capa de luz, en el orden de layer_color_item_t
static const char *const item_names[LAYER_COLOR_COUNT] = {"CIAN", "LOWER", "RAISE", "NUM", "ADJ", "RGB", "CAPS", "MACRO"};

// Muestra en la pantalla el valor recién ajustado: VIA no muestra el número de los deslizadores
static void show_value(const user_settings_t *config, uint8_t value_id) {
    char text[24];
    switch (value_id) {
        case VALUE_ID_RGB_LIMIT:
            snprintf(text, sizeof(text), "TIRA\nBRILL\n%3u%%", config->rgb_limit_percent);
            break;
        case VALUE_ID_RGB_TIMEOUT:
            snprintf(text, sizeof(text), "TIRA\nAPAGA\n%3us", config->rgb_timeout_seconds);
            break;
        case VALUE_ID_DISPLAY_TIMEOUT:
            snprintf(text, sizeof(text), "PANT\nAPAGA\n%3us", config->display.timeout_seconds);
            break;
        case VALUE_ID_DISPLAY_BRIGHTNESS:
            snprintf(text, sizeof(text), "PANT\nBRILL\n%3u%%", config->display.brightness_percent);
            break;
        case VALUE_ID_DISPLAY_ANIM_STEP:
            snprintf(text, sizeof(text), "ANIM\nVELOC\n%3ums", config->display.anim_step_10ms * 10);
            break;
        case VALUE_ID_DISPLAY_ANIMATION: {
            static const char *const names[ANIMATION_COUNT] = {"NINGU", "CULEB", "PACMA", "ALTER"};
            snprintf(text, sizeof(text), "ANIM\n%s", names[config->display.animation]);
            break;
        }
        case VALUE_ID_DISPLAY_ANIM_PAUSE:
            snprintf(text, sizeof(text), "ANIM\nPAUSA\n%2u.%us", config->display.anim_pause_tenths / 10, config->display.anim_pause_tenths % 10);
            break;
        case VALUE_ID_DISPLAY_BLINKS:
            snprintf(text, sizeof(text), "PARP\n %u/s", config->display.blinks_per_second);
            break;
        default: {
            const layer_color_t *color = &config->colors[(value_id - 1) / 2];
            if (value_id % 2 == 0) {
                snprintf(text, sizeof(text), "%-5sH:%3uS:%3u", item_names[(value_id - 1) / 2], color->hue, color->sat);
            } else {
                snprintf(text, sizeof(text), "%-5s%s", item_names[(value_id - 1) / 2], color->enabled ? "SI" : "NO");
            }
            break;
        }
    }
    display_show_overlay(text);
}
#endif

static uint8_t clamp(uint8_t value, uint8_t min, uint8_t max) {
    return value < min ? min : (value > max ? max : value);
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data... ]
    uint8_t *command_id = &data[0];
    uint8_t  channel_id = data[1];
    uint8_t  value_id   = data[2];
    uint8_t *value_data = &data[3];

    if (channel_id != id_custom_channel) {
        *command_id = id_unhandled;
        return;
    }
    if (*command_id == id_custom_save) {
        settings_save();
        return;
    }

    user_settings_t config = *settings();
    uint8_t        *byte   = byte_setting(&config, value_id);
    bool            is_color_item = value_id >= 1 && value_id <= LAYER_COLOR_COUNT * 2;
    if (byte == NULL && !is_color_item) {
        *command_id = id_unhandled;
        return;
    }

    layer_color_t *color    = is_color_item ? &config.colors[(value_id - 1) / 2] : NULL;
    bool           is_color = is_color_item && value_id % 2 == 0;

    switch (*command_id) {
        case id_custom_get_value:
            if (byte != NULL) {
                value_data[0] = *byte;
            } else if (is_color) {
                value_data[0] = color->hue;
                value_data[1] = color->sat;
            } else {
                value_data[0] = color->enabled;
            }
            break;
        case id_custom_set_value:
            if (byte != NULL) {
                *byte = value_data[0];
                // Límites para que un valor raro no deje la tira o la pantalla inservibles
                config.rgb_limit_percent          = clamp(config.rgb_limit_percent, RGB_LIMIT_MIN_PERCENT, 100);
                config.display.brightness_percent = clamp(config.display.brightness_percent, 0, 100);
                config.display.blinks_per_second  = clamp(config.display.blinks_per_second, 0, DISPLAY_MAX_BLINKS);
                config.display.animation          = clamp(config.display.animation, 0, ANIMATION_COUNT - 1);
            } else if (is_color) {
                color->hue = value_data[0];
                color->sat = value_data[1];
            } else {
                color->enabled = value_data[0] != 0;
            }
            settings_update(&config);
#ifdef OLED_ENABLE
            show_value(&config, value_id);
#endif
            break;
        default:
            *command_id = id_unhandled;
            break;
    }
}
