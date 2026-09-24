#include "maflorezp.h"

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    // Pantallas de 128x32 montadas en vertical
    return OLED_ROTATION_270;
}

static const char *layer_name(void) {
    switch (get_highest_layer(layer_state | default_layer_state)) {
        case _QWERTY:
            return "QWRT ";
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

static void render_status(void) {
    oled_write_ln_P(PSTR("SOFLE"), false);
    oled_write_ln_P(PSTR("-----"), false);
    oled_write_ln(layer_name(), false);
    oled_write_ln_P(PSTR(""), false);

    led_t leds = host_keyboard_led_state();
    oled_write_P(PSTR("CAPS "), leds.caps_lock || is_caps_word_on());
    oled_write_P(PSTR("NUM  "), leds.num_lock);
    oled_write_ln_P(PSTR(""), false);
    oled_write_ln(is_keyboard_master() ? "USB  " : "     ", false);
}

bool oled_task_user(void) {
    oled_clear();
#ifdef PIN_SCAN_ENABLE
    oled_write(pin_scan_report(), false);
    return false;
#endif
    const char *message = raw_hid_message();
    if (message[0] != '\0') {
        oled_write(message, false);
        return false;
    }
    render_status();
    return false;
}
