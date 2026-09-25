#include "maflorezp.h"
#include "transactions.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif
#include <string.h>

// Configuración editable desde VIA: colores por capa, techo de brillo de la tira y ajustes de la
// pantalla. Se guarda en el bloque de usuario de la EEPROM y la mitad USB se la envía a la otra
// por el cable cuando cambia y cada pocos segundos, por si la otra mitad se reinició.

#define SETTINGS_MAGIC 0xCA
#define SYNC_INTERVAL_MS 3000

typedef struct {
    uint8_t         magic;
    user_settings_t data;
} stored_settings_t;

_Static_assert(sizeof(stored_settings_t) == EECONFIG_USER_DATA_SIZE, "EECONFIG_USER_DATA_SIZE no coincide con la configuración guardada");
_Static_assert(sizeof(user_settings_t) <= RPC_M2S_BUFFER_SIZE, "La configuración no cabe en un mensaje entre mitades");

static const user_settings_t defaults = {
    .colors =
        {
            [LAYER_COLOR_MANUAL]  = {true, 128, 255}, // cian
            [LAYER_COLOR_LOWER]   = {true, 191, 255}, // morado
            [LAYER_COLOR_RAISE]   = {true, 170, 255}, // azul
            [LAYER_COLOR_NUMERIC] = {true, 0, 255},   // rojo
            [LAYER_COLOR_ADJUST]  = {true, 36, 255},  // dorado
            [LAYER_COLOR_RGB]     = {true, 0, 0},     // blanco
            [LAYER_COLOR_CAPS]    = {true, 85, 255},  // verde
            [LAYER_COLOR_MACRO]   = {true, 21, 255},  // naranja
        },
    .rgb_limit_percent   = 60,
    .rgb_timeout_seconds = 60,
    .display =
        {
            .timeout_seconds    = 15,
            .brightness_percent = 50,
            .animation          = ANIMATION_ALTERNATE,
            .anim_step_10ms     = 3,  // 30 ms por píxel
            .anim_pause_tenths  = 10, // cada carril queda apagado 1 s tras pasar
            .blinks_per_second  = 1,
        },
};

static stored_settings_t stored;
static bool              sync_needed = true;
static uint32_t          sync_timer  = 0;

// Versión de la estructura de la configuración: las herramientas del PC la consultan antes de
// restaurar un respaldo, para no escribir valores de una estructura distinta
uint8_t settings_schema(void) {
    return SETTINGS_MAGIC;
}

const user_settings_t *settings(void) {
    return &stored.data;
}

// Cada módulo toma de la configuración lo que le toca
static void apply(void) {
    rgb_apply_settings();
#ifdef OLED_ENABLE
    display_apply_settings();
#endif
}

void settings_update(const user_settings_t *updated) {
    stored.data = *updated;
    apply();
    sync_needed = true;
}

void settings_save(void) {
    eeconfig_update_user_datablock(&stored, 0, sizeof(stored));
}

static bool is_valid(const user_settings_t *data) {
    return data->rgb_limit_percent >= RGB_LIMIT_MIN_PERCENT && data->rgb_limit_percent <= 100 && data->display.brightness_percent <= 100 && data->display.blinks_per_second <= DISPLAY_MAX_BLINKS &&
           data->display.animation < ANIMATION_COUNT;
}

static void settings_slave_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(user_settings_t)) {
        memcpy(&stored.data, in_data, sizeof(user_settings_t));
        apply();
    }
}

void settings_init(void) {
    eeconfig_read_user_datablock(&stored, 0, sizeof(stored));
    if (stored.magic != SETTINGS_MAGIC || !is_valid(&stored.data)) {
        stored.magic = SETTINGS_MAGIC;
        stored.data  = defaults;
        settings_save();
#ifdef VIA_ENABLE
        // La zona de VIA (keymap y macros) va después de este bloque: si su tamaño cambió, sus
        // datos quedaron desplazados. VIA solo valida con la fecha de compilación, así que en
        // builds del mismo día no lo detecta: se reinicia aquí al keymap compilado.
        eeconfig_init_via();
#endif
    }
    apply();
    transaction_register_rpc(RPC_ID_USER_SETTINGS, settings_slave_handler);
}

void settings_task(void) {
    if (!is_keyboard_master()) {
        return;
    }
    if (sync_needed || timer_elapsed32(sync_timer) > SYNC_INTERVAL_MS) {
        if (transaction_rpc_send(RPC_ID_USER_SETTINGS, sizeof(user_settings_t), &stored.data)) {
            sync_needed = false;
        }
        sync_timer = timer_read32();
    }
}
