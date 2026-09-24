#include "maflorezp.h"

// Diagnóstico: busca dispositivos I2C (la OLED responde en 0x3C o 0x3D) probando por software
// cada par de GPIO libres como SDA/SCL, en los dos sentidos. El resultado sale por la consola,
// así que la mitad a diagnosticar debe ser la conectada al USB.
// Se excluyen GP0 (datos de la tira LED), GP1 (cable entre mitades) y los pines de la matriz.

static const pin_t candidates[] = {GP2, GP3, GP4, GP5, GP11, GP12, GP13, GP14, GP15, GP16, GP17, GP18, GP19};
#define CANDIDATE_COUNT ARRAY_SIZE(candidates)

#define FIRST_SCAN_DELAY_MS 4000
#define SCAN_INTERVAL_MS 15000
#define HALF_BIT_US 5

static uint32_t scan_timer = 0;
static bool     first_scan = true;

// Bus de colector abierto emulado: "alto" es soltar la línea (pull-up), "bajo" es forzarla a 0
static void line_release(pin_t pin) {
    gpio_set_pin_input_high(pin);
}

static void line_low(pin_t pin) {
    gpio_set_pin_output(pin);
    gpio_write_pin_low(pin);
}

static void i2c_start(pin_t sda, pin_t scl) {
    line_release(sda);
    line_release(scl);
    wait_us(HALF_BIT_US);
    line_low(sda);
    wait_us(HALF_BIT_US);
    line_low(scl);
    wait_us(HALF_BIT_US);
}

static void i2c_stop(pin_t sda, pin_t scl) {
    line_low(sda);
    wait_us(HALF_BIT_US);
    line_release(scl);
    wait_us(HALF_BIT_US);
    line_release(sda);
    wait_us(HALF_BIT_US);
}

// Envía un byte y devuelve true si el dispositivo responde con ACK
static bool i2c_write_byte(pin_t sda, pin_t scl, uint8_t byte) {
    for (int8_t bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            line_release(sda);
        } else {
            line_low(sda);
        }
        wait_us(HALF_BIT_US);
        line_release(scl);
        wait_us(HALF_BIT_US);
        line_low(scl);
    }
    line_release(sda);
    wait_us(HALF_BIT_US);
    line_release(scl);
    wait_us(HALF_BIT_US);
    bool ack = !gpio_read_pin(sda);
    line_low(scl);
    wait_us(HALF_BIT_US);
    return ack;
}

static void scan_pair(pin_t sda, pin_t scl) {
    line_release(sda);
    line_release(scl);
    wait_us(50);
    // Con el bus libre las dos líneas deben quedar en alto; si no, hay algo que las fuerza a 0
    if (!gpio_read_pin(sda) || !gpio_read_pin(scl)) {
        return;
    }

    uint8_t found[8];
    uint8_t found_count = 0;
    for (uint8_t address = 0x08; address < 0x78; address++) {
        i2c_start(sda, scl);
        bool ack = i2c_write_byte(sda, scl, address << 1);
        i2c_stop(sda, scl);
        if (ack && found_count < ARRAY_SIZE(found)) {
            found[found_count++] = address;
        }
    }
    if (found_count == 0) {
        return;
    }
    // Si responden muchísimas direcciones no es un dispositivo real: la línea SDA está pegada a 0
    uprintf("I2C SDA=GP%u SCL=GP%u ->", (unsigned)sda, (unsigned)scl);
    for (uint8_t i = 0; i < found_count; i++) {
        uprintf(" 0x%02X", found[i]);
    }
    uprintf(found_count == ARRAY_SIZE(found) ? " (demasiadas: sospechoso)\n" : "\n");
}

static void scan_all(void) {
    uprintf("I2C escaneo inicio (%s)\n", is_keyboard_left() ? "izquierda" : "derecha");
    for (uint8_t i = 0; i < CANDIDATE_COUNT; i++) {
        for (uint8_t j = 0; j < CANDIDATE_COUNT; j++) {
            if (i != j) {
                scan_pair(candidates[i], candidates[j]);
            }
        }
    }
    for (uint8_t i = 0; i < CANDIDATE_COUNT; i++) {
        line_release(candidates[i]);
    }
    uprintf("I2C escaneo fin\n");
}

void i2c_scan_task(void) {
    uint32_t wait_ms = first_scan ? FIRST_SCAN_DELAY_MS : SCAN_INTERVAL_MS;
    if (timer_elapsed32(scan_timer) > wait_ms) {
        scan_all();
        first_scan = false;
        scan_timer = timer_read32();
    }
}
