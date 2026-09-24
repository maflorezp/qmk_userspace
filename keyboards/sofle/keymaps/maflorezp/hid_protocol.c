#include "maflorezp.h"
#include "raw_hid.h"
#include <string.h>

// Protocolo Raw HID PC <-> teclado (paquetes de 32 bytes; el byte 0 es el comando).
// La respuesta siempre repite el comando en el byte 0.
enum raw_hid_command {
    RAW_CMD_PING       = 0x01, // respuesta: 'M','F', versión del protocolo
    RAW_CMD_GET_LAYER  = 0x02, // respuesta: capa más alta activa
    RAW_CMD_OLED_TEXT  = 0x10, // bytes 1..n: texto para la OLED de la mitad USB (se muestra 5 s)
    RAW_CMD_OLED_CLEAR = 0x11, // borra el texto y vuelve a la pantalla de estado
};

#define RAW_PROTOCOL_VERSION 1
#define RAW_PACKET_SIZE 32
#define MESSAGE_DURATION_MS 5000

static char     message[RAW_PACKET_SIZE] = "";
static uint32_t message_timer            = 0;

const char *raw_hid_message(void) {
    if (message[0] != '\0' && timer_elapsed32(message_timer) > MESSAGE_DURATION_MS) {
        message[0] = '\0';
    }
    return message;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t response[RAW_PACKET_SIZE] = {0};
    response[0]                       = data[0];

    switch (data[0]) {
        case RAW_CMD_PING:
            response[1] = 'M';
            response[2] = 'F';
            response[3] = RAW_PROTOCOL_VERSION;
            break;
        case RAW_CMD_GET_LAYER:
            response[1] = get_highest_layer(layer_state | default_layer_state);
            break;
        case RAW_CMD_OLED_TEXT:
            memcpy(message, data + 1, length - 1);
            message[length - 1] = '\0';
            message_timer       = timer_read32();
            break;
        case RAW_CMD_OLED_CLEAR:
            message[0] = '\0';
            break;
        default:
            response[0] = 0xFF;
            break;
    }
    raw_hid_send(response, length);
}
