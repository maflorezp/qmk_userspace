#pragma once

// El converter solo define los nombres del Pro Micro (D2, F4...); los GPxx del RP2040 vienen de aquí
#include "vendors/RP/_pin_defs.h"

// --- Pines reales del Pico (verificados en el firmware de 2022 que corre en el teclado) ---
#undef MATRIX_ROW_PINS
#define MATRIX_ROW_PINS { GP10, GP6, GP7, GP8, GP9 }
#undef MATRIX_COL_PINS
#define MATRIX_COL_PINS { GP28, GP20, GP27, GP26, GP21, GP22 }

// Encoder: la izquierda no tiene; la derecha va a GP2/GP3 (confirmado con PIN_SCAN).
// Con A=GP2 y B=GP3 el giro horario baja (Av Pág), que es el sentido esperado.
#undef ENCODER_A_PINS
#define ENCODER_A_PINS { GP2 }
#undef ENCODER_B_PINS
#define ENCODER_B_PINS { GP3 }
#undef ENCODER_A_PINS_RIGHT
#define ENCODER_A_PINS_RIGHT { GP2 }
#undef ENCODER_B_PINS_RIGHT
#define ENCODER_B_PINS_RIGHT { GP3 }

// OLED en el bus I2C0 (GP16 = SDA, GP17 = SCL)
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP16
#define I2C1_SCL_PIN GP17
#define OLED_TIMEOUT 30000

// --- Teclado partido ---
// Cada mitad sabe de qué lado es por lo grabado en su EEPROM (se graba al flashear con flash.sh).
#define EE_HANDS
// Arreglo del bloqueo al encender: el PC puede tardar más de 2 s en enumerar el USB. Se le da más
// margen y, si aun así las dos mitades quedan como esclavas, el watchdog las reinicia y reintenta.
#define SPLIT_USB_TIMEOUT 5000
#define SPLIT_WATCHDOG_ENABLE
#define SPLIT_WATCHDOG_TIMEOUT 6000
// La mitad esclava necesita saber la capa y el estado de los LEDs para su OLED
#define SPLIT_LAYER_STATE_ENABLE
#define SPLIT_LED_STATE_ENABLE

// --- Tira LED: 4 por mitad ---
#undef RGBLIGHT_LED_COUNT
#define RGBLIGHT_LED_COUNT 8
#undef RGBLED_SPLIT
#define RGBLED_SPLIT { 4, 4 }
#define RGBLIGHT_LED_MAP { 3, 2, 1, 0, 4, 5, 6, 7 }
#define RGBLIGHT_LAYERS
#define RGBLIGHT_MAX_LAYERS 8
#define RGBLIGHT_HUE_STEP 4
#define RGBLIGHT_SAT_STEP 4
#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_RAINBOW_MOOD
#define RGBLIGHT_EFFECT_RAINBOW_SWIRL
#define RGBLIGHT_EFFECT_SNAKE
#define RGBLIGHT_EFFECT_KNIGHT
#define RGBLIGHT_EFFECT_CHRISTMAS
#define RGBLIGHT_EFFECT_STATIC_GRADIENT
#define RGBLIGHT_EFFECT_ALTERNATING
#define RGBLIGHT_EFFECT_TWINKLE

// --- Comportamiento de teclas ---
#undef TAPPING_TERM
#define TAPPING_TERM 200
#define ONESHOT_TAP_TOGGLE 5
#define ONESHOT_TIMEOUT 2000
#define BOTH_SHIFTS_TURNS_ON_CAPS_WORD
#define CAPS_WORD_IDLE_TIMEOUT 3000

#define TRI_LAYER_LOWER_LAYER 1
#define TRI_LAYER_UPPER_LAYER 2
#define TRI_LAYER_ADJUST_LAYER 4
