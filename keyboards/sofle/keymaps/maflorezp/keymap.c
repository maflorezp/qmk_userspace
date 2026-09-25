#include "maflorezp.h"

// Combinaciones de modificadores para las teclas one-shot
#define M_CS   (MOD_LCTL | MOD_LSFT)
#define M_CA   (MOD_LCTL | MOD_LALT)
#define M_AS   (MOD_LALT | MOD_LSFT)
#define M_CAS  (MOD_LCTL | MOD_LALT | MOD_LSFT)
#define M_WCA  (MOD_RGUI | MOD_LCTL | MOD_LALT)
#define M_WCAS MOD_HYPR

bool is_recording_macro = false;

tap_dance_action_t tap_dance_actions[] = {
    [TD_MODS_LEFT]  = ACTION_DUAL_MODS(MODS_CTRL_SHIFT, MODS_CTRL_ALT),
    [TD_MODS_RIGHT] = ACTION_DUAL_MODS(MODS_CTRL_ALT, MODS_CTRL_SHIFT),
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

[_QWERTY] = LAYOUT(
      KC_ESC,        KC_1,        KC_2,        KC_3,        KC_4,        KC_5,                                  KC_6,        KC_7,        KC_8,        KC_9,        KC_0,  OSM(M_CAS),
      KC_TAB,        KC_Q,        KC_W,        KC_E,        KC_R,        KC_T,                                  KC_Y,        KC_U,        KC_I,        KC_O,        KC_P,     KC_BSPC,
TD(TD_MODS_LEFT),     KC_A,        KC_S,        KC_D,        KC_F,        KC_G,                                  KC_H,        KC_J,        KC_K,        KC_L,     KC_SCLN, TD(TD_MODS_RIGHT),
     KC_LSFT,        KC_Z,        KC_X,        KC_C,        KC_V,        KC_B,     KC_MPLY,     KC_MUTE,        KC_N,        KC_M,     KC_COMM,      KC_DOT,     KC_SLSH,     KC_RSFT,
                  KC_LCTL,     KC_LGUI,     KC_LALT,     TL_LOWR,      KC_ENT,                                KC_SPC,     TL_UPPR,     KC_RALT,      KC_APP,     KC_RCTL
),

[_LOWER] = LAYOUT(
     _______,      KC_GRV, RALT(KC_QUOT),   KC_UNDS,     KC_TILD, LCA(KC_PSCR),                              XXXXXXX,      KC_INS,     KC_HOME,     KC_PGUP,     KC_PSCR, TO(_QWERTY),
     _______,     KC_DQUO,     KC_QUOT,     KC_LPRN,     KC_RPRN,      KC_EQL,                            C(KC_PSCR),      KC_DEL,      KC_END,     KC_PGDN,     KC_CALC,     _______,
     _______,     KC_LABK,     KC_RABK,     KC_LBRC,     KC_RBRC,     KC_MINS,                            A(KC_PSCR),     KC_LEFT,       KC_UP,     KC_RGHT,    KC_COLON,     _______,
     _______,     KC_PIPE,     KC_PLUS,     KC_LCBR,     KC_RCBR,     KC_BSLS,     _______,      CK_MIC,  S(KC_PSCR),     KC_LEFT,     KC_DOWN,     KC_RGHT, KC_QUESTION,     _______,
                  _______,     _______,     _______,     _______,      KC_SPC,                                KC_ENT,     _______,     _______,     _______,     _______
),

[_RAISE] = LAYOUT(
     _______,     C(KC_Z),     C(KC_X),     C(KC_C),     C(KC_V),     XXXXXXX,                               KC_PSCR,     KC_SCRL,     KC_PAUS,      KC_F11,      KC_F12, TO(_QWERTY),
     _______,       KC_F1,       KC_F2,       KC_F3,       KC_F4,       KC_F5,                                 KC_F6,       KC_F7,       KC_F8,       KC_F9,      KC_F10,     _______,
     _______,        KC_1,        KC_2,        KC_3,        KC_4,        KC_5,                                  KC_6,        KC_7,        KC_8,        KC_9,        KC_0,     _______,
     _______,     KC_EXLM,       KC_AT,     KC_HASH,      KC_DLR,     KC_PERC,     KC_MNXT,     KC_EXEC,     KC_CIRC,     KC_AMPR,     KC_ASTR,     KC_LPRN,     KC_RPRN,     _______,
                  _______,     _______,     _______,     _______,     _______,                               _______,     _______,     _______,     _______,     _______
),

[_NUMERIC] = LAYOUT(
     _______,     XXXXXXX,     XXXXXXX,     KC_MPLY,     KC_MUTE,     XXXXXXX,                               XXXXXXX,      KC_NUM,     KC_PSLS,     KC_PAST,     KC_PMNS, TO(_QWERTY),
     _______,     XXXXXXX,     KC_MFFD,     KC_MNXT,     KC_VOLU,     XXXXXXX,                               KC_CALC,       KC_P7,       KC_P8,       KC_P9,     KC_PPLS,     _______,
     _______,     XXXXXXX,     KC_MRWD,     KC_MPRV,     KC_VOLD,     XXXXXXX,                               KC_LPRN,       KC_P4,       KC_P5,       KC_P6,     KC_PEQL,     _______,
     _______,     C(KC_Z),     C(KC_X),     C(KC_C),     C(KC_V),     XXXXXXX,     _______,     _______,     KC_RPRN,       KC_P1,       KC_P2,       KC_P3,     KC_PENT,     _______,
                  _______,     _______,     _______,     _______,     _______,                               _______,     _______,       KC_P0,     KC_PDOT,     KC_PCMM
),

[_ADJUST] = LAYOUT(
     QK_BOOT,     XXXXXXX,     XXXXXXX,     EH_RGHT,     XXXXXXX,     CK_HAND,                               CK_HAND,     XXXXXXX,     TO(_RGB),    XXXXXXX, TO(_NUMERIC), TO(_QWERTY),
      QK_RBT,     DM_REC1,     DM_REC2,     DM_PLY1,     DM_PLY2,     DM_RSTP,                               UG_NEXT,     UG_HUEU,     UG_SATU,     UG_VALU,     XXXXXXX,     XXXXXXX,
      EE_CLR,     XXXXXXX,     XXXXXXX,     KC_CAPS,     XXXXXXX,     KC_BRIU,                               UG_PREV,     UG_HUED,     UG_SATD,     UG_VALD,     XXXXXXX,     XXXXXXX,
      KC_PWR,     KC_WAKE,     KC_SLEP,     XXXXXXX,     XXXXXXX,     KC_BRID,     XXXXXXX,     XXXXXXX,     UG_TOGG,     RGB_M_P,     RGB_M_B,     RGB_M_G,     RGB_M_K,     CK_RGB1,
                OSM(M_CS),  OSM(M_WCA),   OSM(M_AS),     _______,     _______,                               _______,     _______,     _______,     _______,     _______
),

[_RGB] = LAYOUT(
     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,                               CK_HAND,     XXXXXXX,     XXXXXXX,     XXXXXXX, TO(_NUMERIC), TO(_QWERTY),
     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,                               UG_NEXT,     UG_HUEU,     UG_SATU,     UG_VALU,     XXXXXXX,     XXXXXXX,
     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,                               UG_PREV,     UG_HUED,     UG_SATD,     UG_VALD,     XXXXXXX,     XXXXXXX,
     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     UG_TOGG,     RGB_M_P,     RGB_M_B,     RGB_M_G,     RGB_M_K,     CK_RGB1,
                  XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,     XXXXXXX,                               _______,     _______,     _______,     _______,     _______
),
};

#if defined(ENCODER_MAP_ENABLE)
// Índice 0 = encoder izquierdo (no instalado), índice 1 = encoder derecho:
// base = volumen, LOWER = rueda del mouse, RAISE = Re Pág / Av Pág
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_QWERTY]  = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU),  ENCODER_CCW_CW(KC_VOLD, KC_VOLU)       },
    [_LOWER]   = { ENCODER_CCW_CW(KC_UP, KC_DOWN),    ENCODER_CCW_CW(MS_WHLU, MS_WHLD)       },
    [_RAISE]   = { ENCODER_CCW_CW(KC_MPRV, KC_MNXT),  ENCODER_CCW_CW(KC_PGUP, KC_PGDN)       },
    [_NUMERIC] = { ENCODER_CCW_CW(_______, _______),  ENCODER_CCW_CW(_______, _______)       },
    [_ADJUST]  = { ENCODER_CCW_CW(_______, _______),  ENCODER_CCW_CW(UG_VALD, UG_VALU)       },
    [_RGB]     = { ENCODER_CCW_CW(_______, _______),  ENCODER_CCW_CW(UG_HUED, UG_HUEU)       },
};
#endif
// clang-format on

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef CONSOLE_ENABLE
    uprintf("KL: kc: 0x%04X, col: %2u, row: %2u, pressed: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed);
#endif

    switch (keycode) {
        case CK_RGB1:
            if (record->event.pressed) {
                layer_colors_toggle_manual();
            }
            return false;
        case CK_MIC:
            // Win + Ctrl + clic central: el atajo de sxhkd que silencia el micrófono
            if (record->event.pressed) {
                register_mods(MOD_BIT(KC_LGUI) | MOD_BIT(KC_LCTL));
                register_code(MS_BTN3);
            } else {
                unregister_code(MS_BTN3);
                unregister_mods(MOD_BIT(KC_LGUI) | MOD_BIT(KC_LCTL));
            }
            return false;
        case CK_HAND:
            // Invierte el lado grabado en esta mitad y reinicia
            if (record->event.pressed) {
                eeconfig_update_handedness(!eeconfig_read_handedness());
                soft_reset_keyboard();
            }
            return false;
    }
    return true;
}

bool dynamic_macro_record_start_user(int8_t direction) {
    is_recording_macro = true;
    return true;
}

bool dynamic_macro_record_end_user(int8_t direction) {
    is_recording_macro = false;
    return true;
}

void keyboard_post_init_user(void) {
    rgb_init();
    settings_init();
    version_init();
}

void housekeeping_task_user(void) {
    settings_task();
    rgb_task();
    hid_protocol_task();
    version_task();
#ifdef PIN_SCAN_ENABLE
    pin_scan_task();
#endif
#ifdef I2C_SCAN_ENABLE
    i2c_scan_task();
#endif
}

// Al entrar en modo de carga (QK_BOOT) el firmware deja de correr: se deja la tira en rojo y
// "CARGA" en la OLED de la mitad USB, porque los LEDs y la pantalla conservan lo último que recibieron.
// Con el doble toque de reset o Bootmagic no hay aviso: el salto ocurre antes de iniciar la tira.
bool shutdown_user(bool jump_to_bootloader) {
    if (jump_to_bootloader) {
        rgblight_layers = NULL;
        rgblight_enable_noeeprom();
        rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
        rgblight_setrgb(RGB_RED);
#ifdef OLED_ENABLE
        oled_clear();
        oled_write_P(PSTR("CARGA"), true);
        oled_render_dirty(true);
#endif
        wait_ms(10);
    }
    return false;
}
