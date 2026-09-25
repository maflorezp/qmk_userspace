#include "maflorezp.h"
#include "usb_util.h"

// Decide qué mitad es la principal (la conectada al PC), en modo híbrido:
// 1. Si GP24 (USB_VBUS_PIN) ve los 5 V del USB, es principal al instante: así no depende de cuánto
//    tarde el PC en enumerar al encender, que era lo que dejaba el teclado bloqueado.
// 2. Si no los ve, espera hasta MASTER_FALLBACK_TIMEOUT_MS a que el PC la enumere. Hace falta porque
//    en el Pico izquierdo GP24 no detecta el VBUS: sin esta espera, con el USB en la izquierda esa
//    mitad se creería secundaria y no se conectaría.
#define MASTER_FALLBACK_TIMEOUT_MS 5000
#define MASTER_FALLBACK_POLL_MS 10

bool is_keyboard_master_impl(void) {
    bool is_master = usb_vbus_state();
    for (uint16_t waited = 0; !is_master && waited < MASTER_FALLBACK_TIMEOUT_MS; waited += MASTER_FALLBACK_POLL_MS) {
        is_master = usb_connected_state();
        if (!is_master) {
            wait_ms(MASTER_FALLBACK_POLL_MS);
        }
    }
    // Igual que QMK: la mitad secundaria suelta el USB para no quedar a medio enumerar
    if (!is_master) {
        usb_disconnect();
    }
    return is_master;
}
