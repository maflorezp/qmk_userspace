#include "maflorezp.h"
#include "qp_internal.h"
#include "qp_comms.h"
#include "qp_tft_panel.h"
#include <string.h>

// Diagnóstico de la pantalla a color desde el PC (tools/lcd_probe.py), sin recompilar por cada
// prueba. Cada orden llega por Raw HID (hid_protocol.c), se responde al instante y se ejecuta en el
// siguiente ciclo del teclado: algunas tardan más de lo que el PC espera una respuesta.
//
// Las órdenes trabajan sobre la pantalla aunque esté elegida la OLED. Cuando termine el diagnóstico,
// CK_DISP vuelve a dejar todo como lo deja el firmware normal.

#define PROBE_PACKET_SIZE 32
#define PROBE_RAW_MAX_BYTES (PROBE_PACKET_SIZE - 3)

// El controlador maneja 240 columnas y el panel usa las 172 centrales
#define PANEL_WIDTH 172
#define PANEL_HEIGHT 320
#define PANEL_COLUMN_OFFSET 34

// Pines de la pantalla en el orden de los bits de la orden de pines fijos
static const pin_t probe_pins[] = {SPI_SCK_PIN, SPI_MOSI_PIN, LCD_CS_PIN, LCD_DC_PIN, LCD_BACKLIGHT_PIN};

static uint8_t pending[PROBE_PACKET_SIZE];
static bool    has_pending = false;
// Desde la primera orden, la pantalla de estado deja de dibujar hasta el siguiente CK_DISP
static bool in_control = false;
static bool demo_running = false;

bool lcd_probe_in_control(void) {
    return in_control;
}

void lcd_probe_release(void) {
    in_control   = false;
    demo_running = false;
}

bool lcd_probe_receive(const uint8_t *data, uint8_t length) {
    if (data[0] < LCD_PROBE_FIRST || data[0] > LCD_PROBE_LAST) {
        return false;
    }
    memcpy(pending, data, MIN(length, PROBE_PACKET_SIZE));
    has_pending = true;
    return true;
}

// Devuelve SCL y SDA al periférico SPI (las órdenes de pines fijos y de SPI por software se los quitan)
static void restore_spi_pins(void) {
    palSetLineMode(SPI_SCK_PIN, SPI_SCK_PAL_MODE);
    palSetLineMode(SPI_MOSI_PIN, SPI_MOSI_PAL_MODE);
}

static void set_pins(uint8_t levels) {
    for (uint8_t i = 0; i < ARRAY_SIZE(probe_pins); i++) {
        gpio_set_pin_output(probe_pins[i]);
        gpio_write_pin(probe_pins[i], levels & (1 << i));
    }
}

// Reinicia el panel con otra velocidad, modo SPI, orientación y desplazamiento, y dibuja el patrón
static void init_with(const uint8_t *data) {
    tft_panel_dc_reset_painter_device_t *panel = (tft_panel_dc_reset_painter_device_t *)lcd_device();

    panel->spi_dc_reset_config.spi_config.divisor = data[1] | (data[2] << 8);
    panel->spi_dc_reset_config.spi_config.mode    = data[3];
    restore_spi_pins();
    gpio_write_pin_high(LCD_BACKLIGHT_PIN);
    if (!qp_init(lcd_device(), data[4] & 3)) {
        return;
    }
    lcd_panel_setup();
    qp_set_viewport_offsets(lcd_device(), data[5], data[6]);
    qp_power(lcd_device(), true);
    lcd_test_draw(lcd_device());
    qp_flush(lcd_device());
}

// Manda un comando y sus parámetros tal cual, con el SPI configurado en la última orden de reinicio
static void send_raw(const uint8_t *data) {
    uint8_t count = MIN(data[2], PROBE_RAW_MAX_BYTES);
    restore_spi_pins();
    if (!qp_comms_start(lcd_device())) {
        return;
    }
    if (count == 0) {
        qp_comms_command(lcd_device(), data[1]);
    } else {
        qp_comms_command_databuf(lcd_device(), data[1], data + 3, count);
    }
    qp_comms_stop(lcd_device());
}

static void fill(const uint8_t *data) {
    painter_device_t device = lcd_device();
    qp_rect(device, 0, 0, qp_get_width(device) - 1, qp_get_height(device) - 1, data[1], data[2], data[3], true);
    qp_flush(device);
}

// --- SPI por software (modo 0): no usa el periférico SPI1, así se descarta su configuración ---

static inline void bitbang_delay(void) {
    for (volatile uint8_t i = 0; i < 4; i++) {
    }
}

static void bitbang_byte(uint8_t value) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        gpio_write_pin(SPI_MOSI_PIN, value & 0x80);
        value <<= 1;
        bitbang_delay();
        gpio_write_pin_high(SPI_SCK_PIN);
        bitbang_delay();
        gpio_write_pin_low(SPI_SCK_PIN);
    }
}

static void bitbang_command(uint8_t command, const uint8_t *params, uint8_t count) {
    gpio_write_pin_low(LCD_CS_PIN);
    gpio_write_pin_low(LCD_DC_PIN);
    bitbang_byte(command);
    gpio_write_pin_high(LCD_DC_PIN);
    for (uint8_t i = 0; i < count; i++) {
        bitbang_byte(params[i]);
    }
    gpio_write_pin_high(LCD_CS_PIN);
}

// Inicio mínimo del ST7789 escrito a mano y relleno de todo el panel con un color RGB565
static void bitbang_fill(const uint8_t *data) {
    uint8_t color_high = data[1], color_low = data[2];
    bool    invert     = data[3];

    set_pins(0x1C); // SCL y SDA en bajo; CS, DC y la luz de fondo en alto

    bitbang_command(0x01, NULL, 0); // reinicio por software
    wait_ms(150);
    bitbang_command(0x11, NULL, 0); // salir del modo de reposo
    wait_ms(120);
    bitbang_command(0x3A, (const uint8_t[]){0x55}, 1); // 16 bits por píxel
    bitbang_command(0x36, (const uint8_t[]){0x00}, 1); // orientación normal, RGB
    bitbang_command(invert ? 0x21 : 0x20, NULL, 0);     // los paneles IPS suelen pedir colores invertidos
    bitbang_command(0x13, NULL, 0);                     // modo normal
    bitbang_command(0x2A, (const uint8_t[]){0, PANEL_COLUMN_OFFSET, 0, PANEL_COLUMN_OFFSET + PANEL_WIDTH - 1}, 4);
    bitbang_command(0x2B, (const uint8_t[]){0, 0, (PANEL_HEIGHT - 1) >> 8, (PANEL_HEIGHT - 1) & 0xFF}, 4);

    gpio_write_pin_low(LCD_CS_PIN);
    gpio_write_pin_low(LCD_DC_PIN);
    bitbang_byte(0x2C); // escribir en la memoria de imagen
    gpio_write_pin_high(LCD_DC_PIN);
    for (uint32_t i = 0; i < (uint32_t)PANEL_WIDTH * PANEL_HEIGHT; i++) {
        bitbang_byte(color_high);
        bitbang_byte(color_low);
    }
    gpio_write_pin_high(LCD_CS_PIN);

    bitbang_command(0x29, NULL, 0); // encender la imagen
}

// --- Animación: un cuadrado que rebota, para ver el redibujado sin frenar el teclado ---

#define DEMO_SQUARE_SIZE 12
#define DEMO_FRAME_MS 16 // unos 60 cuadros por segundo

static const uint8_t demo_hues[] = {0, 85, 170, 43, 128, 213}; // rojo, verde, azul, amarillo, cian, magenta

static int16_t  demo_x, demo_y, demo_dx, demo_dy;
static uint8_t  demo_hue_index;
static uint32_t demo_timer;

static void demo_rect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t hue, uint8_t sat, uint8_t val) {
    if (width > 0 && height > 0) {
        qp_rect(lcd_device(), x, y, x + width - 1, y + height - 1, hue, sat, val, true);
    }
}

// Borra solo lo que el cuadrado dejó descubierto al moverse: así nunca se ve el fondo a través de él
static void demo_erase_trail(int16_t old_x, int16_t old_y) {
    int16_t size = DEMO_SQUARE_SIZE;
    if (demo_x > old_x) {
        demo_rect(old_x, old_y, MIN(demo_x - old_x, size), size, 0, 0, 0);
    } else if (demo_x < old_x) {
        int16_t width = MIN(old_x - demo_x, size);
        demo_rect(old_x + size - width, old_y, width, size, 0, 0, 0);
    }
    int16_t overlap_left  = MAX(old_x, demo_x);
    int16_t overlap_width = MIN(old_x, demo_x) + size - overlap_left;
    if (demo_y > old_y) {
        demo_rect(overlap_left, old_y, overlap_width, MIN(demo_y - old_y, size), 0, 0, 0);
    } else if (demo_y < old_y) {
        int16_t height = MIN(old_y - demo_y, size);
        demo_rect(overlap_left, old_y + size - height, overlap_width, height, 0, 0, 0);
    }
}

static void demo_start(uint8_t step) {
    painter_device_t device = lcd_device();
    step                    = MAX(step, 1);
    demo_x                  = 20;
    demo_y                  = 40;
    demo_dx                 = step;
    demo_dy                 = step + step / 2;
    demo_hue_index          = 0;
    demo_timer              = timer_read32();
    demo_running            = true;
    qp_rect(device, 0, 0, qp_get_width(device) - 1, qp_get_height(device) - 1, 0, 0, 0, true);
}

static void demo_stop(void) {
    demo_running = false;
    lcd_test_draw(lcd_device());
    qp_flush(lcd_device());
}

static void demo_step(void) {
    int16_t width  = qp_get_width(lcd_device());
    int16_t height = qp_get_height(lcd_device());
    int16_t old_x = demo_x, old_y = demo_y;

    demo_x += demo_dx;
    demo_y += demo_dy;
    // Rebota en los bordes y cambia de color con cada choque
    if (demo_x < 0 || demo_x + DEMO_SQUARE_SIZE > width) {
        demo_dx = -demo_dx;
        demo_x  = MAX(0, MIN(demo_x, width - DEMO_SQUARE_SIZE));
        demo_hue_index++;
    }
    if (demo_y < 0 || demo_y + DEMO_SQUARE_SIZE > height) {
        demo_dy = -demo_dy;
        demo_y  = MAX(0, MIN(demo_y, height - DEMO_SQUARE_SIZE));
        demo_hue_index++;
    }
    demo_rect(demo_x, demo_y, DEMO_SQUARE_SIZE, DEMO_SQUARE_SIZE, demo_hues[demo_hue_index % ARRAY_SIZE(demo_hues)], 255, 255);
    demo_erase_trail(old_x, old_y);
    qp_flush(lcd_device());
}

void lcd_probe_task(void) {
    if (demo_running && timer_elapsed32(demo_timer) >= DEMO_FRAME_MS) {
        demo_timer = timer_read32();
        demo_step();
    }
    if (!has_pending) {
        return;
    }
    has_pending = false;
    in_control  = true;

    switch (pending[0]) {
        case LCD_PROBE_PINS:
            set_pins(pending[1]);
            break;
        case LCD_PROBE_INIT:
            init_with(pending);
            break;
        case LCD_PROBE_RAW:
            send_raw(pending);
            break;
        case LCD_PROBE_FILL:
            fill(pending);
            break;
        case LCD_PROBE_BITBANG:
            demo_running = false;
            bitbang_fill(pending);
            break;
        case LCD_PROBE_DEMO:
            if (pending[1]) {
                demo_start(pending[2]);
            } else {
                demo_stop();
            }
            break;
    }
}
