#include "maflorezp.h"
#include "rgblight_drivers.h"
#include "ws2812.h"

// Driver de la tira: el de QMK (ws2812) con una verificación del índice.
// Bug de QMK: en la mitad derecha, rgblight_led_index() resta el inicio de su rango (4) a los
// LEDs de la otra mitad y el uint8_t da 252..255; ws2812_set_color() escribe ese índice sin
// revisarlo y pisaba la RAM de la EEPROM emulada (corrompía el keymap de VIA). Aquí se descartan
// los índices que no son de esta mitad.
static void set_color_checked(int index, uint8_t red, uint8_t green, uint8_t blue) {
    if (index >= 0 && index < WS2812_LED_COUNT) {
        ws2812_set_color(index, red, green, blue);
    }
}

const rgblight_driver_t rgblight_driver = {
    .init          = ws2812_init,
    .set_color     = set_color_checked,
    .set_color_all = ws2812_set_color_all,
    .flush         = ws2812_flush,
};
