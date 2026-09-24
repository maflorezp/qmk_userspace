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

enum custom_keycodes {
    CK_RGB1 = SAFE_RANGE,
    CK_HAND,
};

extern bool is_recording_macro;

// Texto recibido por Raw HID para mostrarlo en la OLED (vacío si no hay mensaje vigente)
const char *raw_hid_message(void);

#ifdef PIN_SCAN_ENABLE
void        pin_scan_task(void);
const char *pin_scan_report(void);
#endif
