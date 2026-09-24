#include "maflorezp.h"
#include <stdio.h>

// Diagnóstico: vigila los GPIO que no usa el teclado y anota cuáles cambian de estado.
// Al girar el encoder, los dos pines que aparecen son A y B. Se ve en la OLED y en la consola.
static const pin_t candidates[] = {GP2, GP3, GP4, GP5, GP11, GP12, GP13, GP14, GP15, GP18, GP19};
#define CANDIDATE_COUNT ARRAY_SIZE(candidates)

static bool     initialized = false;
static bool     last_state[CANDIDATE_COUNT];
static uint16_t change_count[CANDIDATE_COUNT];
static char     report[96];

void pin_scan_task(void) {
    if (!initialized) {
        for (uint8_t i = 0; i < CANDIDATE_COUNT; i++) {
            gpio_set_pin_input_high(candidates[i]);
        }
        wait_us(50);
        for (uint8_t i = 0; i < CANDIDATE_COUNT; i++) {
            last_state[i] = gpio_read_pin(candidates[i]);
        }
        initialized = true;
        return;
    }
    for (uint8_t i = 0; i < CANDIDATE_COUNT; i++) {
        bool state = gpio_read_pin(candidates[i]);
        if (state != last_state[i]) {
            last_state[i] = state;
            change_count[i]++;
            uprintf("PIN GP%u -> %u (cambios: %u)\n", (unsigned)candidates[i], state, change_count[i]);
        }
    }
}

const char *pin_scan_report(void) {
    // Pantalla de 5 columnas: una línea por pin que haya cambiado, "GPnn" + estado actual
    size_t len = snprintf(report, sizeof(report), "SCAN ");
    for (uint8_t i = 0; i < CANDIDATE_COUNT && len < sizeof(report) - 6; i++) {
        if (change_count[i] > 0) {
            len += snprintf(report + len, sizeof(report) - len, "GP%-2u%c", (unsigned)candidates[i], last_state[i] ? '1' : '0');
        }
    }
    return report;
}
