#include "status_screen.h"

#include <stdio.h>
#include <string.h>

#include "game_font.h"

// --- Distribución (172 x 320). Cada zona se borra y se redibuja completa cuando cambia su dato ---
#define MARGIN 8

#define TIME_Y 14
#define TIME_SCALE 5
#define DATE_Y 58
#define DATE_SCALE 2
#define SEPARATOR_TOP_Y 84

#define LAYER_Y 96
#define LAYER_BLOCK_WIDTH 10
#define LAYER_BLOCK_HEIGHT 28
#define LAYER_NAME_X 26
#define LAYER_NAME_SCALE 3

#define LOCKS_Y 138
#define LOCK_WIDTH 74
#define LOCK_HEIGHT 26
#define LOCK_GAP 8

#define MODS_Y 174
#define MOD_WIDTH 37
#define MOD_HEIGHT 30
#define MOD_GAP 4
#define ICON_SIZE 7
#define ICON_SCALE 3

#define ACTIVITY_Y 214
#define REC_WIDTH 54
#define REC_HEIGHT 22
#define SEPARATOR_BOTTOM_Y 244

#define MESSAGE_Y 254
#define MESSAGE_LINE_HEIGHT 18
#define MESSAGE_SCALE 2
#define MESSAGE_LINE_CHARS 14

#define FOOTER_Y 298
#define VERSION_Y 302

#define TEXT_SCALE 2

// --- Colores ---
#define COLOR_DIM_BOX ((game_color_t){0, 0, 40})
#define COLOR_DIM_TEXT ((game_color_t){0, 0, 110})
#define COLOR_SEPARATOR ((game_color_t){0, 0, 60})
#define COLOR_ACTIVE_BOX ((game_color_t){0, 0, 235})
#define COLOR_CAPS ((game_color_t){21, 255, 255})
#define COLOR_NUM ((game_color_t){128, 255, 220})
#define COLOR_REC ((game_color_t){0, 255, 220})
#define COLOR_REC_OFF_PHASE ((game_color_t){0, 255, 70})
#define COLOR_USB ((game_color_t){85, 255, 200})
#define COLOR_VERSION ((game_color_t){0, 0, 150})
#define COLOR_WARNING ((game_color_t){0, 255, 255})

static const char *const weekday_names[] = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};

// Íconos de 7x7 de los modificadores (bit 6 = columna izquierda), en el orden de la fila:
// Win (cuatro cuadros), Ctrl (⌃), Alt (⌥) y Shift (⇧)
static const uint8_t mod_icons[4][ICON_SIZE] = {
    {0x77, 0x77, 0x77, 0x00, 0x77, 0x77, 0x77},
    {0x08, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x00},
    {0x67, 0x10, 0x10, 0x08, 0x04, 0x04, 0x03},
    {0x08, 0x14, 0x22, 0x77, 0x14, 0x14, 0x1C},
};
static const uint8_t mod_bits[4] = {STATUS_MOD_GUI, STATUS_MOD_CTRL, STATUS_MOD_ALT, STATUS_MOD_SHIFT};

static status_state_t last;
static bool           needs_full_draw = true;

void status_screen_reset(void) {
    needs_full_draw = true;
}

uint8_t status_weekday(uint16_t year, uint8_t month, uint8_t day) {
    // Método de Sakamoto
    static const uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year--;
    }
    return (year + year / 4 - year / 100 + year / 400 + offsets[(month - 1) % 12] + day) % 7;
}

static int16_t centered_x(const char *text, uint8_t scale, int16_t left, int16_t width) {
    return left + (width - game_font_text_width(text, scale)) / 2;
}

static void draw_icon(int16_t x, int16_t y, const uint8_t *rows, game_color_t color) {
    for (uint8_t row = 0; row < ICON_SIZE; row++) {
        for (uint8_t column = 0; column < ICON_SIZE; column++) {
            if (rows[row] & (1 << (ICON_SIZE - 1 - column))) {
                game_host_fill_rect(x + column * ICON_SCALE, y + row * ICON_SCALE, ICON_SCALE, ICON_SCALE, color);
            }
        }
    }
}

// --- Zonas ---

static void draw_time_part(int16_t x, const char *text) {
    game_font_draw_text(x, TIME_Y, text, TIME_SCALE, GAME_COLOR_WHITE, GAME_COLOR_BLACK);
}

// "HH:MM" en tres partes, para que el parpadeo de los ":" redibuje solo los dos puntos
static void draw_time(const status_state_t *state, bool digits_changed) {
    int16_t left = centered_x("00:00", TIME_SCALE, 0, GAME_SCREEN_WIDTH);
    if (digits_changed) {
        char text[4];
        if (state->clock_set) {
            snprintf(text, sizeof(text), "%02u", state->hour % 100);
        } else {
            strcpy(text, "--");
        }
        draw_time_part(left, text);
        if (state->clock_set) {
            snprintf(text, sizeof(text), "%02u", state->minute % 100);
        }
        draw_time_part(left + 3 * GAME_FONT_ADVANCE * TIME_SCALE, text);
    }
    draw_time_part(left + 2 * GAME_FONT_ADVANCE * TIME_SCALE, state->colon_visible || !state->clock_set ? ":" : " ");
}

static void draw_date(const status_state_t *state) {
    char text[12];
    if (state->clock_set) {
        snprintf(text, sizeof(text), "%s %02u/%02u", weekday_names[state->weekday % 7], state->day % 100, state->month % 100);
    } else {
        strcpy(text, "--- --/--");
    }
    game_host_fill_rect(0, DATE_Y, GAME_SCREEN_WIDTH, game_font_text_height(DATE_SCALE), GAME_COLOR_BLACK);
    game_font_draw_text(centered_x(text, DATE_SCALE, 0, GAME_SCREEN_WIDTH), DATE_Y, text, DATE_SCALE, COLOR_DIM_TEXT, GAME_COLOR_BLACK);
}

static void draw_layer(const status_state_t *state) {
    game_host_fill_rect(MARGIN, LAYER_Y, LAYER_BLOCK_WIDTH, LAYER_BLOCK_HEIGHT, state->layer_color);
    int16_t name_y = LAYER_Y + (LAYER_BLOCK_HEIGHT - game_font_text_height(LAYER_NAME_SCALE)) / 2;
    game_host_fill_rect(LAYER_NAME_X, LAYER_Y, GAME_SCREEN_WIDTH - LAYER_NAME_X, LAYER_BLOCK_HEIGHT, GAME_COLOR_BLACK);
    game_font_draw_text(LAYER_NAME_X, name_y, state->layer_name, LAYER_NAME_SCALE, state->layer_color, GAME_COLOR_BLACK);
}

static void draw_box_label(int16_t x, int16_t y, int16_t width, int16_t height, const char *label, bool active, game_color_t active_color) {
    game_color_t box  = active ? active_color : COLOR_DIM_BOX;
    game_color_t text = active ? GAME_COLOR_BLACK : COLOR_DIM_TEXT;
    game_host_fill_rect(x, y, width, height, box);
    int16_t text_y = y + (height - game_font_text_height(TEXT_SCALE)) / 2;
    game_font_draw_text(centered_x(label, TEXT_SCALE, x, width), text_y, label, TEXT_SCALE, text, box);
}

static void draw_locks(const status_state_t *state) {
    draw_box_label(MARGIN, LOCKS_Y, LOCK_WIDTH, LOCK_HEIGHT, "CAPS", state->caps_lock, COLOR_CAPS);
    draw_box_label(MARGIN + LOCK_WIDTH + LOCK_GAP, LOCKS_Y, LOCK_WIDTH, LOCK_HEIGHT, "NUM", state->num_lock, COLOR_NUM);
}

static void draw_mods(const status_state_t *state) {
    int16_t icon_width = ICON_SIZE * ICON_SCALE;
    for (uint8_t i = 0; i < 4; i++) {
        bool    active = state->mods & mod_bits[i];
        int16_t x      = MARGIN + i * (MOD_WIDTH + MOD_GAP);
        game_host_fill_rect(x, MODS_Y, MOD_WIDTH, MOD_HEIGHT, active ? COLOR_ACTIVE_BOX : COLOR_DIM_BOX);
        draw_icon(x + (MOD_WIDTH - icon_width) / 2, MODS_Y + (MOD_HEIGHT - icon_width) / 2, mod_icons[i], active ? GAME_COLOR_BLACK : COLOR_DIM_TEXT);
    }
}

static void draw_rec(const status_state_t *state) {
    if (!state->recording) {
        draw_box_label(MARGIN, ACTIVITY_Y, REC_WIDTH, REC_HEIGHT, "REC", false, COLOR_REC);
        return;
    }
    game_color_t box    = state->rec_visible ? COLOR_REC : COLOR_REC_OFF_PHASE;
    int16_t      text_y = ACTIVITY_Y + (REC_HEIGHT - game_font_text_height(TEXT_SCALE)) / 2;
    game_host_fill_rect(MARGIN, ACTIVITY_Y, REC_WIDTH, REC_HEIGHT, box);
    game_font_draw_text(centered_x("REC", TEXT_SCALE, MARGIN, REC_WIDTH), text_y, "REC", TEXT_SCALE, GAME_COLOR_WHITE, box);
}

static void draw_wpm(const status_state_t *state) {
    char text[8];
    snprintf(text, sizeof(text), "WPM %u", (unsigned)state->wpm);
    int16_t width  = game_font_text_width(text, TEXT_SCALE);
    int16_t text_y = ACTIVITY_Y + (REC_HEIGHT - game_font_text_height(TEXT_SCALE)) / 2;
    game_font_draw_text(GAME_SCREEN_WIDTH - MARGIN - width, text_y, text, TEXT_SCALE, GAME_COLOR_WHITE, GAME_COLOR_BLACK);
}

// Parte el mensaje en dos líneas de hasta MESSAGE_LINE_CHARS, cortando en un espacio si se puede
static void draw_message(const status_state_t *state) {
    game_host_fill_rect(0, MESSAGE_Y, GAME_SCREEN_WIDTH, 2 * MESSAGE_LINE_HEIGHT, GAME_COLOR_BLACK);

    const char *rest = state->message;
    for (uint8_t line = 0; line < 2 && *rest != '\0'; line++) {
        size_t length = strlen(rest);
        size_t cut    = length;
        if (length > MESSAGE_LINE_CHARS) {
            cut = MESSAGE_LINE_CHARS;
            for (size_t i = MESSAGE_LINE_CHARS; i > 0; i--) {
                if (rest[i] == ' ') {
                    cut = i;
                    break;
                }
            }
        }
        char text[MESSAGE_LINE_CHARS + 1];
        memcpy(text, rest, cut);
        text[cut] = '\0';
        game_font_draw_text(centered_x(text, MESSAGE_SCALE, 0, GAME_SCREEN_WIDTH), MESSAGE_Y + line * MESSAGE_LINE_HEIGHT, text, MESSAGE_SCALE, GAME_COLOR_WHITE, GAME_COLOR_BLACK);
        rest += cut;
        while (*rest == ' ') {
            rest++;
        }
    }
}

static void draw_footer(const status_state_t *state) {
    game_host_fill_rect(0, FOOTER_Y, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT - FOOTER_Y, GAME_COLOR_BLACK);
    game_font_draw_text(MARGIN, FOOTER_Y, "USB", TEXT_SCALE, state->usb ? COLOR_USB : COLOR_DIM_BOX, GAME_COLOR_BLACK);

    // Si la versión no cabe junto a "USB", se quita el tipo de build ("-normal")
    char version[STATUS_VERSION_SIZE];
    snprintf(version, sizeof(version), "%s", state->version);
    int16_t room = GAME_SCREEN_WIDTH - 2 * MARGIN - game_font_text_width("USB", TEXT_SCALE) - MARGIN;
    char   *dash = strchr(version, '-');
    if (game_font_text_width(version, 1) > room && dash != NULL) {
        *dash = '\0';
    }
    int16_t x = GAME_SCREEN_WIDTH - MARGIN - game_font_text_width(version, 1);
    game_font_draw_text(x, VERSION_Y, version, 1, state->version_warning ? COLOR_WARNING : COLOR_VERSION, GAME_COLOR_BLACK);
}

void status_screen_draw(const status_state_t *state) {
    bool full = needs_full_draw;
    if (full) {
        game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
        game_host_fill_rect(MARGIN, SEPARATOR_TOP_Y, GAME_SCREEN_WIDTH - 2 * MARGIN, 1, COLOR_SEPARATOR);
        game_host_fill_rect(MARGIN, SEPARATOR_BOTTOM_Y, GAME_SCREEN_WIDTH - 2 * MARGIN, 1, COLOR_SEPARATOR);
        needs_full_draw = false;
    }

    bool clock_changed = full || state->clock_set != last.clock_set;
    bool digits_changed = clock_changed || state->hour != last.hour || state->minute != last.minute;
    if (digits_changed || state->colon_visible != last.colon_visible) {
        draw_time(state, digits_changed);
    }
    if (clock_changed || state->day != last.day || state->month != last.month || state->weekday != last.weekday) {
        draw_date(state);
    }
    bool layer_color_changed = memcmp(&state->layer_color, &last.layer_color, sizeof(game_color_t)) != 0;
    if (full || layer_color_changed || last.layer_name == NULL || strcmp(state->layer_name, last.layer_name) != 0) {
        draw_layer(state);
    }
    if (full || state->caps_lock != last.caps_lock || state->num_lock != last.num_lock) {
        draw_locks(state);
    }
    if (full || state->mods != last.mods) {
        draw_mods(state);
    }
    if (full || state->recording != last.recording || (state->recording && state->rec_visible != last.rec_visible)) {
        draw_rec(state);
    }
    if (full || state->wpm != last.wpm) {
        draw_wpm(state);
    }
    if (full || strcmp(state->message, last.message) != 0) {
        draw_message(state);
    }
    if (full || state->usb != last.usb || state->version_warning != last.version_warning || strcmp(state->version, last.version) != 0) {
        draw_footer(state);
    }
    last = *state;
}
