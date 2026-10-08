#include "game_ui.h"

#include "game_font.h"

#define OVERLAY_WIDTH 146
#define OVERLAY_HEIGHT 84
#define OVERLAY_BORDER 2
#define OVERLAY_LINE_HEIGHT 22
#define OVERLAY_PADDING 12
#define OVERLAY_LINE_COUNT 3

static const game_color_t overlay_color = {43, 255, 255};

void game_ui_draw_overlay(int16_t center_y, const char *line_1, const char *line_2, const char *line_3) {
    int16_t x = (GAME_SCREEN_WIDTH - OVERLAY_WIDTH) / 2;
    int16_t y = center_y - OVERLAY_HEIGHT / 2;

    game_host_fill_rect(x, y, OVERLAY_WIDTH, OVERLAY_HEIGHT, overlay_color);
    game_host_fill_rect(x + OVERLAY_BORDER, y + OVERLAY_BORDER, OVERLAY_WIDTH - 2 * OVERLAY_BORDER, OVERLAY_HEIGHT - 2 * OVERLAY_BORDER, GAME_COLOR_BLACK);

    const char *lines[OVERLAY_LINE_COUNT] = {line_1, line_2, line_3};
    for (uint8_t i = 0; i < OVERLAY_LINE_COUNT; i++) {
        int16_t      text_x = (GAME_SCREEN_WIDTH - game_font_text_width(lines[i], GAME_UI_TEXT_SCALE)) / 2;
        game_color_t color  = i == 0 ? overlay_color : GAME_COLOR_WHITE;
        game_font_draw_text(text_x, y + OVERLAY_PADDING + i * OVERLAY_LINE_HEIGHT, lines[i], GAME_UI_TEXT_SCALE, color, GAME_COLOR_BLACK);
    }
}
