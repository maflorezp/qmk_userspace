#include "maflorezp.h"

// Reloj del teclado. El RP2040 no tiene batería, así que la fecha y la hora se reciben del PC
// (Raw HID) o se ajustan a mano; a partir de ahí se avanza con el temporizador interno.
// Hasta recibirlas, la fecha y la hora se consideran desconocidas.

static clock_time_t base;              // fecha y hora en el momento de fijarla
static uint32_t     base_timer = 0;    // temporizador en ese mismo momento
static bool         is_set     = false;

static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (month == 2 && is_leap_year(year)) ? 29 : days[(month - 1) % 12];
}

bool clock_is_set(void) {
    return is_set;
}

void clock_set(const clock_time_t *time) {
    base       = *time;
    base_timer = timer_read32();
    is_set     = true;
}

clock_time_t clock_now(void) {
    clock_time_t now = base;
    if (!is_set) {
        return now;
    }
    uint32_t elapsed = timer_elapsed32(base_timer) / 1000;
    uint32_t seconds = now.second + elapsed;
    now.second       = seconds % 60;
    uint32_t minutes = now.minute + seconds / 60;
    now.minute       = minutes % 60;
    uint32_t hours   = now.hour + minutes / 60;
    now.hour         = hours % 24;

    // Avanza los días que hayan pasado, respetando meses y años bisiestos
    for (uint32_t days = hours / 24; days > 0; days--) {
        if (++now.day > days_in_month(now.year, now.month)) {
            now.day = 1;
            if (++now.month > 12) {
                now.month = 1;
                now.year++;
            }
        }
    }
    return now;
}
