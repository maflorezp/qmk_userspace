#include "maflorezp.h"
#include "raw_hid.h"
#include <string.h>

// Protocolo Raw HID propio PC <-> teclado (paquetes de 32 bytes; el byte 0 es el comando).
// Usa el rango 0x80+ para no chocar con los comandos de VIA (0x01-0x15), que comparten el canal.
// La respuesta siempre repite el comando en el byte 0.
enum custom_hid_command {
    HID_CMD_PING       = 0x80, // respuesta: 'M','F', versión del protocolo
    HID_CMD_GET_LAYER  = 0x81, // respuesta: capa más alta activa
    HID_CMD_VERSION    = 0x82, // respuesta: versión de la mitad USB (texto)
    HID_CMD_OTHER_HALF = 0x83, // respuesta: estado (0 desconocida, 1 igual, 2 distinta) + versión de la otra mitad
    HID_CMD_USB_SIDE   = 0x84, // respuesta: 1 si la mitad USB es la izquierda, 0 si es la derecha
    HID_CMD_SET_TIME   = 0x85, // bytes 1..7: año (2 bytes, little endian), mes, día, hora, minuto, segundo
    HID_CMD_SCHEMA     = 0x86, // respuesta: versión de la estructura de la configuración propia
    HID_CMD_BOOTLOADER = 0x87, // bytes 1..2 = 'B','L': la mitad USB entra en modo de carga tras responder
    HID_CMD_OLED_TEXT  = 0x90, // bytes 1..n: texto para la OLED de la mitad USB (se muestra 5 s)
    HID_CMD_OLED_CLEAR = 0x91, // borra el texto y vuelve a la pantalla de estado
};

#define HID_PROTOCOL_VERSION 7
#define HID_PACKET_SIZE 32
#define MESSAGE_DURATION_MS 5000

// Retardo antes de saltar al bootloader, para que la respuesta alcance a llegar al PC
#define BOOTLOADER_DELAY_MS 100

static char     message[HID_PACKET_SIZE] = "";
static bool     bootloader_requested     = false;
static uint32_t bootloader_timer         = 0;
static uint32_t message_timer            = 0;

const char *raw_hid_message(void) {
    if (message[0] != '\0' && timer_elapsed32(message_timer) > MESSAGE_DURATION_MS) {
        message[0] = '\0';
    }
    return message;
}

// Atiende un comando propio y responde; devuelve false si el comando no es nuestro
static bool handle_custom_command(uint8_t *data, uint8_t length) {
    if (data[0] < HID_CMD_PING) {
        return false;
    }

    uint8_t response[HID_PACKET_SIZE] = {0};
    response[0]                       = data[0];

    switch (data[0]) {
        case HID_CMD_PING:
            response[1] = 'M';
            response[2] = 'F';
            response[3] = HID_PROTOCOL_VERSION;
            break;
        case HID_CMD_GET_LAYER:
            response[1] = get_highest_layer(layer_state | default_layer_state);
            break;
        case HID_CMD_VERSION:
            strncpy((char *)response + 1, firmware_version(), HID_PACKET_SIZE - 2);
            break;
        case HID_CMD_OTHER_HALF:
            response[1] = version_status();
            strncpy((char *)response + 2, other_half_version(), HID_PACKET_SIZE - 3);
            break;
        case HID_CMD_USB_SIDE:
            response[1] = is_keyboard_left();
            break;
        case HID_CMD_SET_TIME: {
            clock_time_t time = {
                .year   = data[1] | (data[2] << 8),
                .month  = data[3],
                .day    = data[4],
                .hour   = data[5],
                .minute = data[6],
                .second = data[7],
            };
            bool valid = time.month >= 1 && time.month <= 12 && time.day >= 1 && time.day <= 31 && time.hour < 24 && time.minute < 60 && time.second < 60;
            if (valid) {
                clock_set(&time);
            }
            response[1] = valid;
            break;
        }
        case HID_CMD_SCHEMA:
            response[1] = settings_schema();
            break;
        case HID_CMD_BOOTLOADER:
            // Se exige la firma 'B','L' para que un paquete suelto no reinicie el teclado
            if (data[1] == 'B' && data[2] == 'L') {
                bootloader_requested = true;
                bootloader_timer     = timer_read32();
                response[1]          = 1;
            }
            break;
        case HID_CMD_OLED_TEXT:
            memcpy(message, data + 1, length - 1);
            message[length - 1] = '\0';
            message_timer       = timer_read32();
#ifdef OLED_ENABLE
            // La pantalla puede estar apagada por inactividad: el mensaje la enciende
            oled_on();
#endif
            break;
        case HID_CMD_OLED_CLEAR:
            message[0] = '\0';
            break;
        default:
            response[0] = 0xFF;
            break;
    }
    raw_hid_send(response, length);
    return true;
}

#ifdef VIA_ENABLE
// VIA es dueño de raw_hid_receive(); nos consulta primero por cada paquete
bool via_command_kb(uint8_t *data, uint8_t length) {
    return handle_custom_command(data, length);
}
#else
void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (!handle_custom_command(data, length)) {
        uint8_t response[HID_PACKET_SIZE] = {0xFF};
        raw_hid_send(response, length);
    }
}
#endif

// Salta al bootloader (misma ruta que QK_BOOT: tira en rojo y "CARGA" en la OLED) cuando pasó el retardo
void hid_protocol_task(void) {
    if (bootloader_requested && timer_elapsed32(bootloader_timer) > BOOTLOADER_DELAY_MS) {
        reset_keyboard();
    }
}
