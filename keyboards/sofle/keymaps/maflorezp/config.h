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
#ifdef OLED_TEST_ENABLE
#    define OLED_TIMEOUT 0
#    define OLED_BRIGHTNESS 255
#else
// El apagado de QMK cuenta el tiempo sin cambios en la pantalla, no sin teclear: con los ":"
// parpadeando nunca se apagaría. El apagado lo maneja oled.c según la configuración de VIA.
#    define OLED_TIMEOUT 0
#endif

// --- Identificación USB ---
// El VID/PID no cambia (VIA reconoce el teclado por ahí); el número de serie lleva la versión
#undef PRODUCT
#define PRODUCT "Sofle Pico"
#define SERIAL_NUMBER "maflorezp-" BUILD_ID "-" BUILD_VARIANT

// --- Teclado partido ---
// Cada mitad sabe de qué lado es por lo grabado en su EEPROM (se graba al flashear con flash.sh).
#define EE_HANDS
// Arreglo del bloqueo al encender: la mitad principal se decide por la presencia de 5 V en VBUS
// (GP24 del Pico, con su divisor) y no esperando a que el PC enumere el USB, que al arrancar puede
// tardar más que el tiempo de espera. La otra mitad recibe los 5 V por VSYS a través del cable
// (su pin VBUS no está soldado), así que su GP24 queda en bajo. Si aun así las dos mitades quedaran
// como esclavas, el watchdog las reinicia.
#define USB_VBUS_PIN GP24
#define SPLIT_WATCHDOG_ENABLE
// La mitad esclava necesita saber la capa y el estado de los LEDs para su OLED
#define SPLIT_LAYER_STATE_ENABLE
#define SPLIT_LED_STATE_ENABLE

// --- Tira LED: 4 por mitad ---
#undef RGBLIGHT_LED_COUNT
#define RGBLIGHT_LED_COUNT 8
#undef RGBLED_SPLIT
#define RGBLED_SPLIT { 4, 4 }
#define RGBLIGHT_LED_MAP { 3, 2, 1, 0, 4, 5, 6, 7 }
// Con el driver propio de la tira (rgb_driver.c) ws2812.h ya no define el tamaño del buffer
#ifdef RGBLIGHT_CUSTOM
#    define WS2812_LED_COUNT RGBLIGHT_LED_COUNT
#endif
#define RGBLIGHT_LAYERS
#define RGBLIGHT_MAX_LAYERS 8
// Las capas de luz usan el brillo elegido en VIA (Lighting) en vez de uno fijo
#define RGBLIGHT_LAYERS_RETAIN_VAL
// Techo de brillo configurable desde VIA (por defecto 60 %): QMK solo lo compara en tiempo de
// ejecución, así que puede ser una variable en vez de una constante
#ifndef __ASSEMBLER__
#    include <stdint.h>
extern uint8_t rgblight_limit_val;
#endif
#define RGBLIGHT_LIMIT_VAL rgblight_limit_val
// Configuración en la EEPROM (settings.c): 1 byte de control + 8 colores x (activo, tono, saturación)
// + techo de brillo y apagado de la tira + 6 ajustes de la pantalla
#define EECONFIG_USER_DATA_SIZE 33
// La configuración ya no cabe en los 32 bytes por defecto de un mensaje entre mitades
#define RPC_M2S_BUFFER_SIZE 48
// Mensajes entre mitades: la configuración editada en VIA y la versión del firmware
#define SPLIT_TRANSACTION_IDS_USER RPC_ID_USER_SETTINGS, RPC_ID_USER_VERSION
// Sin esto la mitad sin USB no se entera de que se teclea: su OLED se apagaba a los 30 s y no volvía
#define SPLIT_ACTIVITY_ENABLE
// La otra mitad también muestra los modificadores presionados en su pantalla
#define SPLIT_MODS_ENABLE
#define SPLIT_WPM_ENABLE
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

// VIA: las 6 capas del keymap quedan editables desde usevia.app
#define DYNAMIC_KEYMAP_LAYER_COUNT 6

#define TRI_LAYER_LOWER_LAYER 1
#define TRI_LAYER_UPPER_LAYER 2
#define TRI_LAYER_ADJUST_LAYER 4
