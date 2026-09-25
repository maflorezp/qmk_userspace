#include "maflorezp.h"

// Colores de la tira por capa (la configuración vive en settings.c y se edita desde VIA).
// Diseño original: las capas momentáneas pintan 3 LEDs por lado y dejan el cuarto como testigo
// con el efecto base; las capas fijas (NUMERIC, RGB), el cian manual y la grabación de macro
// pintan la tira completa. El orden del enum es la prioridad: el último gana.

// Techo de brillo que usa rgblight (RGBLIGHT_LIMIT_VAL apunta aquí); 0-255
uint8_t rgblight_limit_val = 153;

static bool manual_on = false;

// Segmentos en RAM (no const) para poder cambiarles el color en caliente
#define PARTIAL_SEGMENTS RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_WHITE}, {5, 3, HSV_WHITE})
#define FULL_SEGMENTS RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_WHITE})

static rgblight_segment_t manual_segments[]  = FULL_SEGMENTS;
static rgblight_segment_t lower_segments[]   = PARTIAL_SEGMENTS;
static rgblight_segment_t raise_segments[]   = PARTIAL_SEGMENTS;
static rgblight_segment_t numeric_segments[] = FULL_SEGMENTS;
static rgblight_segment_t adjust_segments[]  = PARTIAL_SEGMENTS;
static rgblight_segment_t rgb_segments[]     = FULL_SEGMENTS;
static rgblight_segment_t caps_segments[]    = PARTIAL_SEGMENTS;
static rgblight_segment_t macro_segments[]   = FULL_SEGMENTS;

static rgblight_segment_t *const segments_by_item[LAYER_COLOR_COUNT] = {
    manual_segments, lower_segments, raise_segments, numeric_segments, adjust_segments, rgb_segments, caps_segments, macro_segments,
};

static const rgblight_segment_t *const rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    manual_segments, lower_segments, raise_segments, numeric_segments, adjust_segments, rgb_segments, caps_segments, macro_segments
);

// Qué pide mostrar cada capa de luz en este momento (antes de aplicar el interruptor "activo")
static bool requested[LAYER_COLOR_COUNT];

static void show(layer_color_item_t item, bool on) {
    requested[item] = on;
    rgblight_set_layer_state(item, on && settings()->colors[item].enabled);
}

void rgb_init(void) {
    rgblight_layers = rgb_layers;
}

// Copia los colores y el techo de brillo de la configuración y redibuja lo visible
void rgb_apply_settings(void) {
    const user_settings_t *config = settings();
    for (uint8_t item = 0; item < LAYER_COLOR_COUNT; item++) {
        for (rgblight_segment_t *segment = segments_by_item[item]; segment->index != RGBLIGHT_END_SEGMENT_INDEX; segment++) {
            segment->hue = config->colors[item].hue;
            segment->sat = config->colors[item].sat;
        }
        show(item, requested[item]);
    }

    uint8_t limit = ((uint16_t)config->rgb_limit_percent * 255) / 100;
    if (limit != rgblight_limit_val) {
        rgblight_limit_val = limit;
        // Redibuja con el brillo actual para que el nuevo techo se note de inmediato
        rgblight_sethsv_noeeprom(rgblight_get_hue(), rgblight_get_sat(), rgblight_get_val());
    }
}

// Apagado por inactividad: solo la mitad USB decide; la otra recibe el estado por RGBLIGHT_SPLIT.
// Si la tira ya estaba apagada a mano (UG_TOGG), no se vuelve a encender sola.
static bool timed_out = false;

void rgb_task(void) {
    if (!is_keyboard_master()) {
        return;
    }
    uint8_t timeout = settings()->rgb_timeout_seconds;
    bool    idle    = timeout > 0 && last_input_activity_elapsed() > timeout * 1000UL;
    if (idle && !timed_out && rgblight_is_enabled()) {
        rgblight_disable_noeeprom();
        timed_out = true;
    } else if (!idle && timed_out) {
        rgblight_enable_noeeprom();
        timed_out = false;
    }
}

void layer_colors_toggle_manual(void) {
    manual_on = !manual_on;
    show(LAYER_COLOR_MANUAL, manual_on);
}

layer_state_t layer_state_set_user(layer_state_t state) {
    show(LAYER_COLOR_LOWER, layer_state_cmp(state, _LOWER));
    show(LAYER_COLOR_RAISE, layer_state_cmp(state, _RAISE));
    show(LAYER_COLOR_NUMERIC, layer_state_cmp(state, _NUMERIC));
    show(LAYER_COLOR_ADJUST, layer_state_cmp(state, _ADJUST));
    show(LAYER_COLOR_RGB, layer_state_cmp(state, _RGB));
    return state;
}

bool led_update_user(led_t led_state) {
    show(LAYER_COLOR_CAPS, led_state.caps_lock);
    return true;
}

void matrix_scan_user(void) {
    if (requested[LAYER_COLOR_MACRO] != is_recording_macro) {
        show(LAYER_COLOR_MACRO, is_recording_macro);
    }
}
