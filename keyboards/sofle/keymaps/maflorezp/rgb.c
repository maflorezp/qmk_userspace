#include "maflorezp.h"
#include "transactions.h"
#include <string.h>

// Colores de la tira por capa, editables desde VIA y guardados en la EEPROM (bloque de usuario).
// Diseño original: las capas momentáneas pintan 3 LEDs por lado y dejan el cuarto como testigo
// con el efecto base; las capas fijas (NUMERIC, RGB), el cian manual y la grabación de macro
// pintan la tira completa. El orden del enum es la prioridad: el último gana.
// También guarda el techo de brillo (porcentaje) que limita toda la tira.

#define LAYER_COLORS_MAGIC 0xC1
#define SYNC_INTERVAL_MS 3000
#define DEFAULT_LIMIT_PERCENT 60
#define MIN_LIMIT_PERCENT 10

// Lo que viaja a la otra mitad: los colores y el techo de brillo
typedef struct {
    layer_color_t items[LAYER_COLOR_COUNT];
    uint8_t       limit_percent;
} layer_color_sync_t;

typedef struct {
    uint8_t            magic;
    layer_color_sync_t data;
} layer_color_config_t;

_Static_assert(sizeof(layer_color_config_t) == EECONFIG_USER_DATA_SIZE, "EECONFIG_USER_DATA_SIZE no coincide con la tabla de colores");

static const layer_color_t default_colors[LAYER_COLOR_COUNT] = {
    [LAYER_COLOR_MANUAL]  = {true, 128, 255}, // cian
    [LAYER_COLOR_LOWER]   = {true, 191, 255}, // morado
    [LAYER_COLOR_RAISE]   = {true, 170, 255}, // azul
    [LAYER_COLOR_NUMERIC] = {true, 0, 255},   // rojo
    [LAYER_COLOR_ADJUST]  = {true, 36, 255},  // dorado
    [LAYER_COLOR_RGB]     = {true, 0, 0},     // blanco
    [LAYER_COLOR_CAPS]    = {true, 85, 255},  // verde
    [LAYER_COLOR_MACRO]   = {true, 21, 255},  // naranja
};

static layer_color_config_t config;

// Techo de brillo que usa rgblight (RGBLIGHT_LIMIT_VAL apunta aquí); 0-255
uint8_t rgblight_limit_val = (DEFAULT_LIMIT_PERCENT * 255) / 100;
static bool                 manual_on   = false;
static bool                 sync_needed = true;
static uint32_t             sync_timer  = 0;

// Segmentos en RAM (no const) para poder cambiarles el color en caliente
#define PARTIAL_SEGMENTS RGBLIGHT_LAYER_SEGMENTS({0, 3, HSV_WHITE}, {5, 3, HSV_WHITE})
#define FULL_SEGMENTS RGBLIGHT_LAYER_SEGMENTS({0, 8, HSV_WHITE})

static rgblight_segment_t manual_segments[]  = FULL_SEGMENTS;
static rgblight_segment_t lower_segments[]   = PARTIAL_SEGMENTS;
static rgblight_segment_t raise_segments[]   = PARTIAL_SEGMENTS;
static rgblight_segment_t numeric_segments[] = FULL_SEGMENTS;
static rgblight_segment_t adjust_segments[]  = PARTIAL_SEGMENTS;
static rgblight_segment_t rgb_segments[]     = FULL_SEGMENTS;
static rgblight_segment_t caps_segments[]    = PARTIAL_SEGMENTS;
static rgblight_segment_t macro_segments[]   = FULL_SEGMENTS;

static rgblight_segment_t *const segments_by_item[LAYER_COLOR_COUNT] = {
    manual_segments, lower_segments, raise_segments, numeric_segments, adjust_segments, rgb_segments, caps_segments, macro_segments,
};

static const rgblight_segment_t *const rgb_layers[] = RGBLIGHT_LAYERS_LIST(
    manual_segments, lower_segments, raise_segments, numeric_segments, adjust_segments, rgb_segments, caps_segments, macro_segments
);

// Qué pide mostrar cada capa de luz en este momento (antes de aplicar el interruptor "activo")
static bool requested[LAYER_COLOR_COUNT];

static void show(layer_color_item_t item, bool on) {
    requested[item] = on;
    rgblight_set_layer_state(item, on && config.data.items[item].enabled);
}

// Copia los colores de la tabla a los segmentos y redibuja las capas visibles
static void apply_colors(void) {
    for (uint8_t item = 0; item < LAYER_COLOR_COUNT; item++) {
        for (rgblight_segment_t *segment = segments_by_item[item]; segment->index != RGBLIGHT_END_SEGMENT_INDEX; segment++) {
            segment->hue = config.data.items[item].hue;
            segment->sat = config.data.items[item].sat;
        }
        show(item, requested[item]);
    }
}

static void apply_limit(void) {
    rgblight_limit_val = ((uint16_t)config.data.limit_percent * 255) / 100;
    // Redibuja con el brillo actual para que el nuevo techo se note de inmediato
    rgblight_sethsv_noeeprom(rgblight_get_hue(), rgblight_get_sat(), rgblight_get_val());
}

static void load_defaults(void) {
    config.magic = LAYER_COLORS_MAGIC;
    memcpy(config.data.items, default_colors, sizeof(config.data.items));
    config.data.limit_percent = DEFAULT_LIMIT_PERCENT;
}

void layer_colors_save(void) {
    eeconfig_update_user_datablock(&config, 0, sizeof(config));
}

static void layer_colors_slave_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(config.data)) {
        memcpy(&config.data, in_data, sizeof(config.data));
        apply_limit();
        apply_colors();
    }
}

void layer_colors_init(void) {
    rgblight_layers = rgb_layers;

    eeconfig_read_user_datablock(&config, 0, sizeof(config));
    if (config.magic != LAYER_COLORS_MAGIC || config.data.limit_percent < MIN_LIMIT_PERCENT || config.data.limit_percent > 100) {
        load_defaults();
        layer_colors_save();
    }
    apply_limit();
    apply_colors();

    transaction_register_rpc(RPC_ID_USER_LAYER_COLORS, layer_colors_slave_handler);
}

void layer_colors_task(void) {
    // La mitad USB le manda la tabla a la otra cuando cambia y cada tanto, por si se reinició
    if (!is_keyboard_master()) {
        return;
    }
    if (sync_needed || timer_elapsed32(sync_timer) > SYNC_INTERVAL_MS) {
        if (transaction_rpc_send(RPC_ID_USER_LAYER_COLORS, sizeof(config.data), &config.data)) {
            sync_needed = false;
        }
        sync_timer = timer_read32();
    }
}

const layer_color_t *layer_colors_get(layer_color_item_t item) {
    return &config.data.items[item];
}

void layer_colors_set(layer_color_item_t item, const layer_color_t *color) {
    config.data.items[item] = *color;
    apply_colors();
    sync_needed = true;
}

uint8_t brightness_limit_get_percent(void) {
    return config.data.limit_percent;
}

void brightness_limit_set_percent(uint8_t percent) {
    config.data.limit_percent = percent < MIN_LIMIT_PERCENT ? MIN_LIMIT_PERCENT : (percent > 100 ? 100 : percent);
    apply_limit();
    sync_needed = true;
}

void layer_colors_toggle_manual(void) {
    manual_on = !manual_on;
    show(LAYER_COLOR_MANUAL, manual_on);
}

layer_state_t layer_state_set_user(layer_state_t state) {
    show(LAYER_COLOR_LOWER, layer_state_cmp(state, _LOWER));
    show(LAYER_COLOR_RAISE, layer_state_cmp(state, _RAISE));
    show(LAYER_COLOR_NUMERIC, layer_state_cmp(state, _NUMERIC));
    show(LAYER_COLOR_ADJUST, layer_state_cmp(state, _ADJUST));
    show(LAYER_COLOR_RGB, layer_state_cmp(state, _RGB));
    return state;
}

bool led_update_user(led_t led_state) {
    show(LAYER_COLOR_CAPS, led_state.caps_lock);
    return true;
}

void matrix_scan_user(void) {
    if (requested[LAYER_COLOR_MACRO] != is_recording_macro) {
        show(LAYER_COLOR_MACRO, is_recording_macro);
    }
}
