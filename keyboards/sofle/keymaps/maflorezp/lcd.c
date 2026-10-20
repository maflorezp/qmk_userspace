#include "maflorezp.h"
#include "qp_comms.h"
#include "status_screen.h"
#include <stdio.h>
#include <string.h>

// Pantalla a color GMT147SPI (1,47" IPS, ST7789P3, 172x320) por SPI1, en la mitad del USB.
// Mientras la OLED siga instalada, CK_DISP alterna entre las dos y la elección se recuerda al
// reiniciar. Muestra la pantalla de estado (screen/status_screen.c); el patrón de prueba
// (lcd_test.c) queda para el diagnóstico por HID (lcd_probe.c).
//
// La elección va en la palabra de 32 bits del teclado en la EEPROM: la de usuario no existe cuando se
// usa el bloque de usuario (settings.c), y el Sofle no usa la del teclado. Así no se toca la
// configuración ni la zona de VIA. Borrar la EEPROM (EE_CLR) la deja en 0, es decir, en la OLED.

#define LCD_WIDTH 172
#define LCD_HEIGHT 320
// El controlador maneja 240 columnas y el panel usa las 172 centrales
#define LCD_OFFSET_X 34
#define LCD_OFFSET_Y 0
// 125 MHz / 8 = 15,6 MHz: probado sin fallas con cables de protoboard (la estática de la primera
// prueba era un panel quebrado, no la velocidad). Se puede subir con lcd_probe.py init --divisor.
#define LCD_SPI_DIVISOR 8
#define LCD_SPI_MODE 3
// Refresco del panel: 0x01 = 111 Hz (de fábrica 0x0F = 60 Hz). Sin la patilla TE no se puede
// sincronizar el dibujo con el barrido; a 111 Hz casi no se alcanza a ver un cuadro a medio pintar.
#define ST7789_FRAME_RATE_CONTROL 0xC6
#define ST7789_FRAME_RATE_111HZ 0x01
// Valor guardado cuando está elegida la pantalla a color; cualquier otro es la OLED
#define LCD_SELECTED_MARK 1

static painter_device_t lcd;
static bool             selected; // lo elegido con CK_DISP

painter_device_t lcd_device(void) {
    return lcd;
}

// Ajustes que el driver de QMK no hace; van después de cada qp_init(), que reinicia el panel
void lcd_panel_setup(void) {
    if (qp_comms_start(lcd)) {
        qp_comms_command_databyte(lcd, ST7789_FRAME_RATE_CONTROL, ST7789_FRAME_RATE_111HZ);
        qp_comms_stop(lcd);
    }
    qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);
}

bool lcd_is_active(void) {
    // Solo la mitad del USB tiene la pantalla
    return selected && is_keyboard_master();
}

// --- Dibujo: la pantalla de estado (screen/status_screen.c) pinta con rectángulos de game_host.h ---

#define LCD_STATUS_INTERVAL_MS 50

static bool ready = false; // el panel respondió a qp_init() y tiene la pantalla de estado

void game_host_fill_rect(int16_t x, int16_t y, int16_t width, int16_t height, game_color_t color) {
    int16_t left   = MAX(x, 0);
    int16_t top    = MAX(y, 0);
    int16_t right  = MIN(x + width, GAME_SCREEN_WIDTH);
    int16_t bottom = MIN(y + height, GAME_SCREEN_HEIGHT);
    if (left < right && top < bottom) {
        qp_rect(lcd, left, top, right - 1, bottom - 1, color.h, color.s, color.v, true);
    }
}

static const char *layer_full_name(uint8_t layer) {
    switch (layer) {
        case _QWERTY:
            return "QWERTY";
        case _LOWER:
            return "LOWER";
        case _RAISE:
            return "RAISE";
        case _NUMERIC:
            return "NUMERIC";
        case _ADJUST:
            return "ADJUST";
        case _RGB:
            return "RGB";
        default:
            return "???";
    }
}

// El mismo color que pone la tira en esa capa; sin color propio, el de la tira en QWERTY
static game_color_t layer_color(uint8_t layer) {
    static const int8_t items[] = {
        [_QWERTY] = -1, [_LOWER] = LAYER_COLOR_LOWER, [_RAISE] = LAYER_COLOR_RAISE, [_NUMERIC] = LAYER_COLOR_NUMERIC, [_ADJUST] = LAYER_COLOR_ADJUST, [_RGB] = LAYER_COLOR_RGB,
    };
    if (layer < ARRAY_SIZE(items) && items[layer] >= 0 && settings()->colors[items[layer]].enabled) {
        const layer_color_t *color = &settings()->colors[items[layer]];
        return (game_color_t){color->hue, color->sat, 255};
    }
    return (game_color_t){rgblight_get_hue(), rgblight_get_sat(), 255};
}

// Alterna según los parpadeos por segundo configurados en VIA; sin parpadeo, siempre visible
static bool blink_visible(void) {
    uint8_t blinks = settings()->display.blinks_per_second;
    return blinks == 0 || (timer_read32() / (500 / blinks)) % 2 == 0;
}

static void read_status(status_state_t *state) {
    memset(state, 0, sizeof(*state));

    state->clock_set = clock_is_set();
    if (state->clock_set) {
        clock_time_t now = clock_now();
        state->hour      = now.hour;
        state->minute    = now.minute;
        state->day       = now.day;
        state->month     = now.month;
        state->weekday   = status_weekday(now.year, now.month, now.day);
    }
    state->colon_visible = blink_visible();

    uint8_t layer      = get_highest_layer(layer_state | default_layer_state);
    state->layer_name  = layer_full_name(layer);
    state->layer_color = layer_color(layer);

    led_t leds       = host_keyboard_led_state();
    state->caps_lock = leds.caps_lock || is_caps_word_on();
    state->num_lock  = leds.num_lock;

    uint8_t mods = get_mods() | get_oneshot_mods();
    state->mods  = ((mods & MOD_MASK_GUI) ? STATUS_MOD_GUI : 0) | ((mods & MOD_MASK_CTRL) ? STATUS_MOD_CTRL : 0) | ((mods & MOD_MASK_ALT) ? STATUS_MOD_ALT : 0) | ((mods & MOD_MASK_SHIFT) ? STATUS_MOD_SHIFT : 0);

    state->recording   = is_recording_macro;
    state->rec_visible = blink_visible();
    state->wpm         = get_current_wpm();

    // El valor que se ajusta en VIA tiene prioridad sobre el mensaje del PC
    const char *message = display_overlay();
    if (message[0] == '\0') {
        message = raw_hid_message();
    }
    snprintf(state->message, sizeof(state->message), "%s", message);

    state->usb             = is_keyboard_master();
    state->version_warning = version_status() != VERSION_MATCH;
    snprintf(state->version, sizeof(state->version), "%s", firmware_version());
}

// Apagado por inactividad con el tiempo de la pantalla en VIA: se apaga solo la luz de fondo y el
// contenido se sigue actualizando sin verse. Un mensaje o un ajuste de VIA la encienden.
static bool backlight_wanted(const status_state_t *state) {
    uint8_t timeout = settings()->display.timeout_seconds;
    return timeout == 0 || last_input_activity_elapsed() <= timeout * 1000UL || state->message[0] != '\0';
}

void lcd_task(void) {
    static uint32_t status_timer = 0;
    if (!lcd_is_active() || !ready || lcd_probe_in_control()) {
        return;
    }
    if (timer_elapsed32(status_timer) < LCD_STATUS_INTERVAL_MS) {
        return;
    }
    status_timer = timer_read32();

    status_state_t state;
    read_status(&state);
    gpio_write_pin(LCD_BACKLIGHT_PIN, backlight_wanted(&state));
    status_screen_draw(&state);
    qp_flush(lcd);
}

// Enciende o apaga la pantalla según lo elegido. Al encender, el panel se inicia de cero y se dibuja
// completo (unos 150 ms en los que no se lee el teclado); después solo se redibuja lo que cambia.
// Si un cable hizo falso contacto, oprimir CK_DISP dos veces lo reintenta.
static void apply(void) {
    ready = false;
    lcd_probe_release();
    if (!lcd_is_active()) {
        qp_power(lcd, false);
        gpio_write_pin_low(LCD_BACKLIGHT_PIN);
        return;
    }
    gpio_write_pin_high(LCD_BACKLIGHT_PIN);
    if (!qp_init(lcd, QP_ROTATION_0)) {
        return;
    }
    lcd_panel_setup();
    qp_power(lcd, true);
    status_screen_reset();
    ready = true;
}

void lcd_init(void) {
    gpio_set_pin_output(LCD_BACKLIGHT_PIN);
    lcd      = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RESET_PIN, LCD_SPI_DIVISOR, LCD_SPI_MODE);
    selected = eeconfig_read_kb() == LCD_SELECTED_MARK;
    apply();
}

void lcd_toggle(void) {
    selected = !selected;
    eeconfig_update_kb(selected ? LCD_SELECTED_MARK : 0);
    apply();
}
