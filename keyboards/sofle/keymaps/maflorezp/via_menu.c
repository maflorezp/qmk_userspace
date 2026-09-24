#include "maflorezp.h"
#include "via.h"

// Menú "Colores por capa" de usevia.app (definido en via/sofle_maflorezp.json).
// Por cada elemento hay dos valores en el canal propio: id impar = activo, id par = color.
//   value_id = elemento * 2 + 1 -> activo (1 byte)
//   value_id = elemento * 2 + 2 -> color (tono, saturación)
// Después de los elementos va el techo de brillo, en porcentaje (1 byte).
#define VALUE_ID_BRIGHTNESS_LIMIT (LAYER_COLOR_COUNT * 2 + 1)

static bool decode_value_id(uint8_t value_id, layer_color_item_t *item, bool *is_color) {
    if (value_id == 0 || value_id > LAYER_COLOR_COUNT * 2) {
        return false;
    }
    *item     = (value_id - 1) / 2;
    *is_color = (value_id % 2) == 0;
    return true;
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data... ]
    uint8_t *command_id = &data[0];
    uint8_t  channel_id = data[1];
    uint8_t *value_data = &data[3];

    if (channel_id == id_custom_channel && data[2] == VALUE_ID_BRIGHTNESS_LIMIT) {
        if (*command_id == id_custom_set_value) {
            brightness_limit_set_percent(value_data[0]);
        } else if (*command_id == id_custom_get_value) {
            value_data[0] = brightness_limit_get_percent();
        } else {
            layer_colors_save();
        }
        return;
    }

    layer_color_item_t item;
    bool               is_color;
    if (channel_id != id_custom_channel || (*command_id != id_custom_save && !decode_value_id(data[2], &item, &is_color))) {
        *command_id = id_unhandled;
        return;
    }

    switch (*command_id) {
        case id_custom_set_value: {
            layer_color_t color = *layer_colors_get(item);
            if (is_color) {
                color.hue = value_data[0];
                color.sat = value_data[1];
            } else {
                color.enabled = value_data[0] != 0;
            }
            layer_colors_set(item, &color);
            break;
        }
        case id_custom_get_value: {
            const layer_color_t *color = layer_colors_get(item);
            if (is_color) {
                value_data[0] = color->hue;
                value_data[1] = color->sat;
            } else {
                value_data[0] = color->enabled;
            }
            break;
        }
        case id_custom_save:
            layer_colors_save();
            break;
        default:
            *command_id = id_unhandled;
            break;
    }
}
