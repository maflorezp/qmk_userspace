#include "maflorezp.h"

// Capas de luz: cada una pinta segmentos de la tira. Las de índice mayor tienen prioridad.
enum rgb_layer_index {
    RGB_LAYER_CYAN,
    RGB_LAYER_LOWER,
    RGB_LAYER_RAISE,
    RGB_LAYER_NUMERIC,
    RGB_LAYER_ADJUST,
    RGB_LAYER_RGB,
    RGB_LAYER_CAPS,
    RGB_LAYER_MACRO,
};

const rgblight_segment_t PROGMEM cyan_layer[]    = RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_CYAN});
const rgblight_segment_t PROGMEM lower_layer[]   = RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_PURPLE}, {5, 3, HSV_PURPLE});
const rgblight_segment_t PROGMEM raise_layer[]   = RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_BLUE}, {5, 3, HSV_BLUE});
const rgblight_segment_t PROGMEM numeric_layer[] = RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_RED});
const rgblight_segment_t PROGMEM adjust_layer[]  = RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_GOLD}, {5, 3, HSV_GOLD});
const rgblight_segment_t PROGMEM rgb_layer[]     = RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_WHITE});
const rgblight_segment_t PROGMEM caps_layer[]    = RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_GREEN}, {5, 3, HSV_GREEN});
const rgblight_segment_t PROGMEM macro_layer[]   = RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_ORANGE});

const rgblight_segment_t *const PROGMEM rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    cyan_layer, lower_layer, raise_layer, numeric_layer, adjust_layer, rgb_layer, caps_layer, macro_layer
);

void keyboard_post_init_user(void) {
    rgblight_layers = rgb_layers;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    rgblight_set_layer_state(RGB_LAYER_LOWER, layer_state_cmp(state, _LOWER));
    rgblight_set_layer_state(RGB_LAYER_RAISE, layer_state_cmp(state, _RAISE));
    rgblight_set_layer_state(RGB_LAYER_NUMERIC, layer_state_cmp(state, _NUMERIC));
    rgblight_set_layer_state(RGB_LAYER_ADJUST, layer_state_cmp(state, _ADJUST));
    rgblight_set_layer_state(RGB_LAYER_RGB, layer_state_cmp(state, _RGB));
    return state;
}

bool led_update_user(led_t led_state) {
    rgblight_set_layer_state(RGB_LAYER_CAPS, led_state.caps_lock);
    return true;
}

void matrix_scan_user(void) {
    rgblight_set_layer_state(RGB_LAYER_MACRO, is_recording_macro);
}
