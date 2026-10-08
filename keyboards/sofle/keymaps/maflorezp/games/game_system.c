#include "game_system.h"

#include <stdio.h>

#include "game_car.h"
#include "game_font.h"
#include "game_tetris.h"
#include "game_ui.h"

#define MENU_TITLE_SCALE 3
#define MENU_TITLE_Y 20
#define MENU_LIST_Y 80
#define MENU_ROW_HEIGHT 30
#define MENU_ROW_MARGIN 8
#define MENU_ROW_PADDING 8
#define MENU_HINT_LINE_HEIGHT 20
#define MENU_HINT_Y (GAME_SCREEN_HEIGHT - 2 * MENU_HINT_LINE_HEIGHT - 8)
// Ningún juego en curso: se está en el menú
#define NO_GAME GAME_ID_COUNT

typedef struct {
    const char *name;
    void (*init)(uint32_t seed, uint32_t now_ms);
    void (*input)(game_input_t input, uint32_t now_ms);
    void (*tick)(uint32_t now_ms);
} game_entry_t;

static const game_entry_t games[GAME_ID_COUNT] = {
    [GAME_ID_CAR]    = {"CARRITOS", game_car_init, game_car_input, game_car_tick},
    [GAME_ID_TETRIS] = {"TETRIS", game_tetris_init, game_tetris_input, game_tetris_tick},
};

static const game_color_t title_color    = {43, 255, 255};
static const game_color_t selected_color = {128, 255, 255};
static const game_color_t record_color   = {0, 0, 140};
static const game_color_t hint_color     = {0, 0, 110};

static game_id_t current_game;
static uint8_t   selected;
static uint32_t  seed_base;

static void draw_menu_row(uint8_t index) {
    bool         is_selected = index == selected;
    game_color_t background  = is_selected ? selected_color : GAME_COLOR_BLACK;
    game_color_t color       = is_selected ? GAME_COLOR_BLACK : GAME_COLOR_WHITE;
    int16_t      y           = MENU_LIST_Y + index * MENU_ROW_HEIGHT;
    int16_t      text_y      = y + (MENU_ROW_HEIGHT - game_font_text_height(GAME_UI_TEXT_SCALE)) / 2;

    game_host_fill_rect(MENU_ROW_MARGIN, y, GAME_SCREEN_WIDTH - 2 * MENU_ROW_MARGIN, MENU_ROW_HEIGHT - 2, background);
    game_font_draw_text(MENU_ROW_MARGIN + MENU_ROW_PADDING, text_y, games[index].name, GAME_UI_TEXT_SCALE, color, background);
}

// Récord del juego señalado, al pie de la lista
static void draw_menu_record(void) {
    char    text[16];
    int16_t y = MENU_LIST_Y + GAME_ID_COUNT * MENU_ROW_HEIGHT + MENU_ROW_MARGIN;

    game_host_fill_rect(0, y, GAME_SCREEN_WIDTH, game_font_text_height(GAME_UI_TEXT_SCALE), GAME_COLOR_BLACK);
    snprintf(text, sizeof(text), "REC %lu", (unsigned long)game_host_record_load(selected));
    game_font_draw_text(MENU_ROW_MARGIN + MENU_ROW_PADDING, y, text, GAME_UI_TEXT_SCALE, record_color, GAME_COLOR_BLACK);
}

static void draw_menu(void) {
    static const char *title = "JUEGOS";

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    game_font_draw_text((GAME_SCREEN_WIDTH - game_font_text_width(title, MENU_TITLE_SCALE)) / 2, MENU_TITLE_Y, title, MENU_TITLE_SCALE, title_color, GAME_COLOR_BLACK);
    for (uint8_t i = 0; i < GAME_ID_COUNT; i++) {
        draw_menu_row(i);
    }
    draw_menu_record();
    game_font_draw_text(MENU_ROW_MARGIN, MENU_HINT_Y, "CLIC: JUGAR", GAME_UI_TEXT_SCALE, hint_color, GAME_COLOR_BLACK);
    game_font_draw_text(MENU_ROW_MARGIN, MENU_HINT_Y + MENU_HINT_LINE_HEIGHT, "ESC: SALIR", GAME_UI_TEXT_SCALE, hint_color, GAME_COLOR_BLACK);
}

static void select_game(uint8_t index) {
    uint8_t previous = selected;
    selected         = index;
    draw_menu_row(previous);
    draw_menu_row(selected);
    draw_menu_record();
}

void game_system_init(uint32_t seed, uint32_t now_ms) {
    (void)now_ms;
    seed_base    = seed;
    current_game = NO_GAME;
    draw_menu();
}

void game_system_start(game_id_t game, uint32_t now_ms) {
    current_game = game;
    selected     = game;
    // El momento en que se entra hace distinta cada partida
    games[game].init(seed_base + now_ms, now_ms);
}

bool game_system_input(game_input_t input, uint32_t now_ms) {
    if (current_game != NO_GAME) {
        if (input == GAME_INPUT_BACK) {
            current_game = NO_GAME;
            draw_menu();
        } else {
            games[current_game].input(input, now_ms);
        }
        return true;
    }

    switch (input) {
        case GAME_INPUT_LEFT:
        case GAME_INPUT_UP:
            select_game((selected + GAME_ID_COUNT - 1) % GAME_ID_COUNT);
            break;
        case GAME_INPUT_RIGHT:
        case GAME_INPUT_DOWN:
            select_game((selected + 1) % GAME_ID_COUNT);
            break;
        case GAME_INPUT_ACTION:
            game_system_start(selected, now_ms);
            break;
        case GAME_INPUT_BACK:
            return false;
    }
    return true;
}

void game_system_tick(uint32_t now_ms) {
    if (current_game != NO_GAME) {
        games[current_game].tick(now_ms);
    }
}
