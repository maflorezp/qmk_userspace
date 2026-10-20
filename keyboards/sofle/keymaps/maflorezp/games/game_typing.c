#include "game_typing.h"

#include <stdio.h>

#include "game_font.h"
#include "game_ui.h"

// Franja superior con el puntaje, las vidas, las letras por minuto y la precisión; debajo, la
// cancha por donde caen las letras hasta el piso
#define HUD_HEIGHT 40
#define HUD_X 4
// La segunda línea es "LPM 045 P 97%": siempre ocupa lo mismo, así cada cifra se repinta en su
// sitio sin tocar el resto de la línea ni dejar restos de la anterior
#define HUD_CHAR_WIDTH (GAME_FONT_ADVANCE * GAME_UI_TEXT_SCALE)
#define HUD_LPM_COLUMN 4
#define HUD_PRECISION_COLUMN 9
#define SEPARATOR_HEIGHT 2
#define FIELD_TOP (HUD_HEIGHT + SEPARATOR_HEIGHT + 2)
#define FLOOR_HEIGHT 4
#define FLOOR_Y (GAME_SCREEN_HEIGHT - FLOOR_HEIGHT)
#define FIELD_CENTER_Y ((FIELD_TOP + FLOOR_Y) / 2)

// Vidas: cuadritos a la derecha de la primera línea de la franja
#define LIVES_START 3
#define LIFE_WIDTH 10
#define LIFE_GAP 4
#define LIVES_X (GAME_SCREEN_WIDTH - HUD_X - LIVES_START * LIFE_WIDTH - (LIVES_START - 1) * LIFE_GAP)

// Las letras se dibujan grandes (15x21 px) en una de 8 columnas fijas; cada columna deja 3 px a
// cada lado de la letra, así dos letras nunca se tocan
#define LETTER_SCALE 3
#define LETTER_WIDTH (GAME_FONT_GLYPH_WIDTH * LETTER_SCALE)
#define LETTER_HEIGHT (GAME_FONT_GLYPH_HEIGHT * LETTER_SCALE)
#define SLOT_COUNT 8
#define SLOT_WIDTH 21
#define SLOTS_X ((GAME_SCREEN_WIDTH - SLOT_COUNT * SLOT_WIDTH) / 2)
#define LETTER_MARGIN ((SLOT_WIDTH - LETTER_WIDTH) / 2)
// Una columna no recibe letra nueva mientras otra no haya bajado este trecho desde arriba
#define SPAWN_CLEARANCE (LETTER_HEIGHT + 8)
// Tampoco se repiten las columnas de las últimas letras que salieron, para repartirlas
#define RECENT_SLOTS 2
#define MAX_LETTERS 8
// Distancia que cae una letra desde que sale hasta que toca el piso
#define FALL_DISTANCE (FLOOR_Y - LETTER_HEIGHT - FIELD_TOP)

// Las letras avanzan un paso cada STEP_MS; su altura va en punto fijo con 8 bits de fracción
#define STEP_MS 16
#define FIXED_SHIFT 8
// Si el juego se atrasa más de estos pasos, se pierde el atraso en vez de correr para alcanzarlo
#define MAX_STEP_LAG 4

// La partida arranca en el tramo de la velocidad elegida (1 a SPEED_MAX) y cada HITS_PER_TIER
// aciertos sube uno más: las letras caen más rápido y salen más seguido, hasta MAX_TIER
#define HITS_PER_TIER 10
#define MAX_TIER 14
#define SPEED_MIN 1
#define SPEED_MAX 9
#define FALL_SPEED_START 22
#define FALL_SPEED_PER_TIER 5
#define SPAWN_INTERVAL_START_MS 1800
#define SPAWN_INTERVAL_PER_TIER_MS 100

// Tiempo que una letra acertada queda resaltada antes de borrarse
#define HIT_FLASH_MS 120
// Tiempo que el piso queda en rojo al perder una vida
#define FLOOR_FLASH_MS 250
// Las letras por minuto se recalculan cada tanto, no con cada tecla
#define LPM_UPDATE_MS 1000
// Las letras por minuto se miden como si hubiera pasado al menos este tiempo: así el primer
// acierto no marca una cifra absurda
#define LPM_MIN_ELAPSED_MS 1000
#define MAX_LPM 999
#define MAX_SCORE 999999UL
#define POINTS_PER_HIT 10

// El recuadro de fin va algo arriba del centro de la cancha
#define OVERLAY_CENTER_Y (FIELD_CENTER_Y - 20)

// Pantalla de configuración: título, una fila por conjunto de caracteres (nombre y, debajo y en
// pequeño, lo que trae), la fila de la velocidad, la de iniciar y el récord al pie
#define SETUP_TITLE_SCALE 3
#define SETUP_TITLE_Y 8
#define SETUP_LIST_Y 36
#define SETUP_ROW_MARGIN 4
#define SETUP_ROW_PADDING 4
#define SETUP_TEXT_X (SETUP_ROW_MARGIN + SETUP_ROW_PADDING)
#define SET_ROW_HEIGHT 34
#define SET_LABEL_OFFSET 4
#define SET_KEYS_OFFSET 22
#define SET_KEYS_SCALE 1
#define OPTION_ROW_HEIGHT 26
// Entre una fila y la siguiente queda una raya negra
#define ROW_GAP 2
#define CHECKBOX_SIZE 12
#define CHECKBOX_BORDER 2
// Margen del relleno de una casilla marcada, medido desde el borde exterior
#define CHECKBOX_INSET 3
#define CHECKBOX_LABEL_GAP 6
#define SET_LABEL_X (SETUP_TEXT_X + CHECKBOX_SIZE + CHECKBOX_LABEL_GAP)
#define SETUP_RECORD_Y 270
#define SETUP_HINT_Y 296

// Las preferencias se guardan en 32 bits: los conjuntos marcados en el byte bajo, la velocidad
// en el siguiente y una marca de formato en el alto. Sin la marca (0 si nunca se guardaron, o
// basura) se usan las de fábrica.
#define PREFERENCES_MARKER 0xA5UL
#define PREFERENCES_MARKER_SHIFT 24
#define PREFERENCES_SPEED_SHIFT 8

static const game_color_t separator_color = {0, 0, 90};
static const game_color_t floor_color     = {0, 0, 90};
static const game_color_t floor_hit_color = {0, 255, 255};
static const game_color_t hud_color       = {0, 0, 200};
static const game_color_t life_color      = {0, 255, 230};
static const game_color_t title_color     = {43, 255, 255};
static const game_color_t selected_color  = {128, 255, 255};
static const game_color_t keys_color      = {0, 0, 160};
static const game_color_t check_color     = {85, 255, 255};
static const game_color_t start_color     = {43, 255, 255};
static const game_color_t record_color    = {0, 0, 140};
static const game_color_t hint_color      = {0, 0, 110};
static const game_color_t hit_color       = {85, 255, 255};

// Color de la letra según lo que lleva caído: blanca, amarilla desde la mitad y roja en el
// último cuarto. Solo se repinta completa al cruzar uno de esos límites.
#define URGENCY_COUNT 3
static const game_color_t urgency_colors[URGENCY_COUNT] = {
    {0, 0, 255},    // blanco
    {43, 255, 255}, // amarillo
    {0, 255, 255},  // rojo
};

typedef struct {
    const char *name;
    // Lo que se ve debajo del nombre para recordar qué teclas entran
    const char *keys;
    const char *characters;
} character_set_t;

// Los textos van en Latin-1, un byte por carácter, igual que lo que llega por game_typing_type().
// Cada \x va en su propio pedazo de cadena: "\xF1" "ABC" en vez de "\xF1ABC", que C leería como
// un solo escape. Ningún carácter está en dos conjuntos, así ninguno sale el doble al juntarlos.
#define SET_COUNT 5
#define SET_CHARACTERS_MAX 32
static const character_set_t character_sets[SET_COUNT] = {
    {"MINUSCULAS", "a-z \xF1", "abcdefghijklmnopqrstuvwxyz\xF1"},
    {"MAYUSCULAS", "A-Z \xD1", "ABCDEFGHIJKLMNOPQRSTUVWXYZ\xD1"},
    {"NUMEROS", "0-9", "0123456789"},
    {"SIMBOLOS", ".,;:-_\xBF?\xA1!()\"'@#$%&/=+*", ".,;:-_\xBF?\xA1!()\"'@#$%&/=+*"},
    {"TILDES", "\xE1\xE9\xED\xF3\xFA\xFC \xC1\xC9\xCD\xD3\xDA\xDC", "\xE1\xE9\xED\xF3\xFA\xFC\xC1\xC9\xCD\xD3\xDA\xDC"},
};
#define SET_LOWERCASE_BIT 0x01

// Filas de la pantalla de configuración: primero los conjuntos, después estas dos
enum setup_row {
    ROW_SPEED = SET_COUNT,
    ROW_START,
    ROW_COUNT,
};

typedef enum {
    STATE_SETUP,
    STATE_PLAYING,
    STATE_OVER,
} typing_state_t;

typedef enum {
    LETTER_FREE,
    LETTER_FALLING,
    // Acertada: queda resaltada un momento y después se borra
    LETTER_HIT,
} letter_state_t;

typedef struct {
    letter_state_t state;
    uint8_t        character;
    uint8_t        slot;
    uint8_t        urgency;
    // Altura en punto fijo; drawn_y es donde quedó pintada
    int32_t        y;
    int16_t        drawn_y;
    uint32_t       hit_ms;
} letter_t;

static typing_state_t state;
// Lo elegido en la configuración: un bit por conjunto y la velocidad inicial
static uint8_t  set_mask;
static uint8_t  speed;
static uint32_t saved_preferences;
static uint8_t  cursor;
// La unión de los conjuntos marcados, armada al empezar cada partida
static uint8_t  alphabet[SET_COUNT * SET_CHARACTERS_MAX];
static uint8_t  alphabet_length;
static uint8_t  set_count;
static letter_t letters[MAX_LETTERS];
static uint8_t  recent_slots[RECENT_SLOTS];
static uint8_t  lives;
static uint32_t score;
static uint32_t record;
static uint32_t hits;
static uint32_t misses;
static uint32_t start_ms;
static uint32_t step_timer;
static uint32_t spawn_timer;
static uint32_t lpm_timer;
static uint32_t floor_flash_ms;
static bool     floor_flashing;
// Lo que muestra la segunda línea de la franja, para repintarla solo cuando cambia
static uint16_t shown_lpm;
static uint8_t  shown_precision;
static uint32_t rng_state;

static uint32_t random_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint8_t tier(void) {
    uint32_t value = (uint32_t)(speed - SPEED_MIN) + hits / HITS_PER_TIER;
    return value > MAX_TIER ? MAX_TIER : (uint8_t)value;
}

// Lo que baja una letra en cada paso, en 1/256 de píxel
static int32_t fall_step(void) {
    int32_t pixels_per_second = FALL_SPEED_START + tier() * FALL_SPEED_PER_TIER;
    return (pixels_per_second << FIXED_SHIFT) * STEP_MS / 1000;
}

static uint32_t spawn_interval(void) {
    return SPAWN_INTERVAL_START_MS - tier() * SPAWN_INTERVAL_PER_TIER_MS;
}

static int16_t letter_x(const letter_t *letter) {
    return SLOTS_X + letter->slot * SLOT_WIDTH + LETTER_MARGIN;
}

static uint16_t letters_per_minute(uint32_t now_ms) {
    uint32_t elapsed = now_ms - start_ms;
    if (elapsed < LPM_MIN_ELAPSED_MS) {
        elapsed = LPM_MIN_ELAPSED_MS;
    }
    uint32_t value = hits * 60000UL / elapsed;
    return value > MAX_LPM ? MAX_LPM : (uint16_t)value;
}

static uint8_t precision(void) {
    uint32_t keys = hits + misses;
    return keys == 0 ? 100 : (uint8_t)(hits * 100 / keys);
}

static void draw_hud_score(void) {
    char text[16];

    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    game_font_draw_text(HUD_X, GAME_UI_HUD_LINE_1_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

static void draw_life(uint8_t index, game_color_t color) {
    game_host_fill_rect(LIVES_X + index * (LIFE_WIDTH + LIFE_GAP), GAME_UI_HUD_LINE_1_Y, LIFE_WIDTH, game_font_text_height(GAME_UI_TEXT_SCALE), color);
}

static void draw_hud_lives(void) {
    for (uint8_t i = 0; i < LIVES_START; i++) {
        draw_life(i, i < lives ? life_color : GAME_COLOR_BLACK);
    }
}

static void draw_hud_field(uint8_t column, const char *text) {
    game_font_draw_text(HUD_X + column * HUD_CHAR_WIDTH, GAME_UI_HUD_LINE_2_Y, text, GAME_UI_TEXT_SCALE, hud_color, GAME_COLOR_BLACK);
}

static void draw_hud_lpm(uint16_t lpm) {
    char text[8];

    shown_lpm = lpm;
    snprintf(text, sizeof(text), "%03u", lpm);
    draw_hud_field(HUD_LPM_COLUMN, text);
}

static void draw_hud_precision(uint8_t percent) {
    char text[8];

    shown_precision = percent;
    snprintf(text, sizeof(text), "%3u%%", percent);
    draw_hud_field(HUD_PRECISION_COLUMN, text);
}

static void draw_hud_stats(void) {
    draw_hud_field(0, "LPM     P");
    draw_hud_lpm(0);
    draw_hud_precision(precision());
}

// La precisión cambia con cada tecla; las letras por minuto se actualizan aparte, cada tanto
static void update_hud_precision(void) {
    uint8_t percent = precision();
    if (percent != shown_precision) {
        draw_hud_precision(percent);
    }
}

static void draw_floor(game_color_t color) {
    game_host_fill_rect(0, FLOOR_Y, GAME_SCREEN_WIDTH, FLOOR_HEIGHT, color);
}

static void erase_letter(letter_t *letter) {
    game_font_draw_glyph(letter_x(letter), letter->drawn_y, letter->character, LETTER_SCALE, GAME_COLOR_BLACK);
    letter->state = LETTER_FREE;
}

static void erase_all_letters(void) {
    for (uint8_t i = 0; i < MAX_LETTERS; i++) {
        if (letters[i].state != LETTER_FREE) {
            erase_letter(&letters[i]);
        }
    }
}

static void finish_game(void) {
    char text[16];

    state = STATE_OVER;
    erase_all_letters();
    if (floor_flashing) {
        floor_flashing = false;
        draw_floor(floor_color);
    }
    if (score > record) {
        record = score;
        game_host_record_save(GAME_ID_TYPING, record);
    }
    snprintf(text, sizeof(text), "PTS %06lu", (unsigned long)score);
    // El clic vuelve a la configuración, para cambiarla o empezar otra
    game_ui_draw_overlay(OVERLAY_CENTER_Y, "FIN", text, "CLIC: SEGUIR");
}

static void lose_life(letter_t *letter, uint32_t now_ms) {
    erase_letter(letter);
    lives--;
    draw_life(lives, GAME_COLOR_BLACK);
    if (lives == 0) {
        finish_game();
        return;
    }
    draw_floor(floor_hit_color);
    floor_flashing = true;
    floor_flash_ms = now_ms;
}

// Una columna está libre si la última letra que salió por ella ya bajó lo suficiente y no es de
// las que salieron hace poco
static bool slot_is_free(uint8_t slot) {
    for (uint8_t i = 0; i < RECENT_SLOTS; i++) {
        if (recent_slots[i] == slot) {
            return false;
        }
    }
    for (uint8_t i = 0; i < MAX_LETTERS; i++) {
        if (letters[i].state != LETTER_FREE && letters[i].slot == slot && letters[i].drawn_y < FIELD_TOP + SPAWN_CLEARANCE) {
            return false;
        }
    }
    return true;
}

// Saca una letra nueva arriba; devuelve false si no cabe (todas ocupadas o sin columna libre)
static bool spawn_letter(void) {
    letter_t *letter = NULL;
    for (uint8_t i = 0; i < MAX_LETTERS; i++) {
        if (letters[i].state == LETTER_FREE) {
            letter = &letters[i];
            break;
        }
    }
    if (letter == NULL) {
        return false;
    }

    uint8_t free_slots[SLOT_COUNT];
    uint8_t free_count = 0;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++) {
        if (slot_is_free(slot)) {
            free_slots[free_count++] = slot;
        }
    }
    if (free_count == 0) {
        return false;
    }

    letter->state        = LETTER_FALLING;
    letter->slot         = free_slots[random_next() % free_count];
    letter->character    = alphabet[random_next() % alphabet_length];
    letter->urgency      = 0;
    letter->y            = (int32_t)FIELD_TOP << FIXED_SHIFT;
    letter->drawn_y      = FIELD_TOP;
    for (uint8_t i = RECENT_SLOTS - 1; i > 0; i--) {
        recent_slots[i] = recent_slots[i - 1];
    }
    recent_slots[0] = letter->slot;
    // La cancha está limpia donde sale la letra: basta con pintar sus píxeles
    game_font_draw_glyph(letter_x(letter), letter->drawn_y, letter->character, LETTER_SCALE, urgency_colors[0]);
    return true;
}

static uint8_t urgency_of(int16_t y) {
    int16_t fallen = y - FIELD_TOP;
    if (fallen >= FALL_DISTANCE * 3 / 4) {
        return 2;
    }
    return fallen >= FALL_DISTANCE / 2 ? 1 : 0;
}

// Baja una letra un paso. Solo se repintan los píxeles que cambian; al cruzar un límite de
// urgencia la letra se repinta entera con su nuevo color.
static void step_letter(letter_t *letter, uint32_t now_ms) {
    letter->y += fall_step();
    int16_t new_y = letter->y >> FIXED_SHIFT;
    if (new_y == letter->drawn_y) {
        return;
    }
    if (new_y + LETTER_HEIGHT > FLOOR_Y) {
        lose_life(letter, now_ms);
        return;
    }

    int16_t x = letter_x(letter);
    game_font_move_glyph(x, letter->drawn_y, new_y, letter->character, LETTER_SCALE, urgency_colors[letter->urgency], GAME_COLOR_BLACK);
    letter->drawn_y = new_y;

    uint8_t urgency = urgency_of(new_y);
    if (urgency != letter->urgency) {
        letter->urgency = urgency;
        game_font_draw_glyph(x, new_y, letter->character, LETTER_SCALE, urgency_colors[urgency]);
    }
}

static void step(uint32_t now_ms) {
    for (uint8_t i = 0; i < MAX_LETTERS && state == STATE_PLAYING; i++) {
        letter_t *letter = &letters[i];
        if (letter->state == LETTER_HIT) {
            if (now_ms - letter->hit_ms >= HIT_FLASH_MS) {
                erase_letter(letter);
            }
        } else if (letter->state == LETTER_FALLING) {
            step_letter(letter, now_ms);
        }
    }
    if (state != STATE_PLAYING) {
        return;
    }

    if (floor_flashing && now_ms - floor_flash_ms >= FLOOR_FLASH_MS) {
        floor_flashing = false;
        draw_floor(floor_color);
    }
    // Si no cupo la letra, se vuelve a probar en el paso siguiente
    if (now_ms - spawn_timer >= spawn_interval() && spawn_letter()) {
        spawn_timer = now_ms;
    }
    if (now_ms - lpm_timer >= LPM_UPDATE_MS) {
        lpm_timer   = now_ms;
        uint16_t lpm = letters_per_minute(now_ms);
        if (lpm != shown_lpm) {
            draw_hud_lpm(lpm);
        }
    }
}

static void reset_round(void) {
    score          = 0;
    lives          = LIVES_START;
    hits           = 0;
    misses         = 0;
    floor_flashing = false;
    for (uint8_t i = 0; i < MAX_LETTERS; i++) {
        letters[i].state = LETTER_FREE;
    }
    // Ninguna columna cuenta como reciente al empezar
    for (uint8_t i = 0; i < RECENT_SLOTS; i++) {
        recent_slots[i] = SLOT_COUNT;
    }
}

// Junta en alphabet los caracteres de los conjuntos marcados
static void build_alphabet(void) {
    alphabet_length = 0;
    set_count       = 0;
    for (uint8_t i = 0; i < SET_COUNT; i++) {
        if (!(set_mask & (1 << i))) {
            continue;
        }
        set_count++;
        for (const char *c = character_sets[i].characters; *c != '\0'; c++) {
            alphabet[alphabet_length++] = (uint8_t)*c;
        }
    }
}

static uint32_t pack_preferences(void) {
    return (PREFERENCES_MARKER << PREFERENCES_MARKER_SHIFT) | ((uint32_t)speed << PREFERENCES_SPEED_SHIFT) | set_mask;
}

// Toma las preferencias guardadas; si no tienen la marca o traen algo imposible, las de fábrica:
// solo minúsculas y la velocidad más lenta
static void load_preferences(void) {
    uint32_t preferences = game_host_typing_preferences_load();
    uint8_t  mask        = preferences & 0xFF;
    uint8_t  value       = (preferences >> PREFERENCES_SPEED_SHIFT) & 0xFF;

    saved_preferences = preferences;
    if ((preferences >> PREFERENCES_MARKER_SHIFT) != PREFERENCES_MARKER || mask == 0 || mask >= (1 << SET_COUNT) || value < SPEED_MIN || value > SPEED_MAX) {
        set_mask = SET_LOWERCASE_BIT;
        speed    = SPEED_MIN;
        return;
    }
    set_mask = mask;
    speed    = value;
}

static int16_t setup_row_y(uint8_t row) {
    if (row < SET_COUNT) {
        return SETUP_LIST_Y + row * SET_ROW_HEIGHT;
    }
    return SETUP_LIST_Y + SET_COUNT * SET_ROW_HEIGHT + (row - SET_COUNT) * OPTION_ROW_HEIGHT;
}

// Casilla de un conjunto: cuadrito con borde, relleno si está marcada
static void draw_checkbox(int16_t y, bool checked, game_color_t color, game_color_t background) {
    game_host_fill_rect(SETUP_TEXT_X, y, CHECKBOX_SIZE, CHECKBOX_SIZE, color);
    game_host_fill_rect(SETUP_TEXT_X + CHECKBOX_BORDER, y + CHECKBOX_BORDER, CHECKBOX_SIZE - 2 * CHECKBOX_BORDER, CHECKBOX_SIZE - 2 * CHECKBOX_BORDER, background);
    if (checked) {
        game_host_fill_rect(SETUP_TEXT_X + CHECKBOX_INSET, y + CHECKBOX_INSET, CHECKBOX_SIZE - 2 * CHECKBOX_INSET, CHECKBOX_SIZE - 2 * CHECKBOX_INSET, color);
    }
}

// Pinta una fila completa; la del cursor va resaltada como en el menú de juegos
static void draw_setup_row(uint8_t row) {
    bool         is_selected = row == cursor;
    game_color_t background  = is_selected ? selected_color : GAME_COLOR_BLACK;
    game_color_t color       = is_selected ? GAME_COLOR_BLACK : GAME_COLOR_WHITE;
    int16_t      y           = setup_row_y(row);
    int16_t      height      = row < SET_COUNT ? SET_ROW_HEIGHT : OPTION_ROW_HEIGHT;
    int16_t      text_y      = y + (height - ROW_GAP - game_font_text_height(GAME_UI_TEXT_SCALE)) / 2;
    char         text[16];

    game_host_fill_rect(SETUP_ROW_MARGIN, y, GAME_SCREEN_WIDTH - 2 * SETUP_ROW_MARGIN, height - ROW_GAP, background);
    if (row < SET_COUNT) {
        const character_set_t *set     = &character_sets[row];
        int16_t                label_y = y + SET_LABEL_OFFSET;
        draw_checkbox(label_y + 1, set_mask & (1 << row), is_selected ? GAME_COLOR_BLACK : check_color, background);
        game_font_draw_text(SET_LABEL_X, label_y, set->name, GAME_UI_TEXT_SCALE, color, background);
        game_font_draw_text(SET_LABEL_X, y + SET_KEYS_OFFSET, set->keys, SET_KEYS_SCALE, is_selected ? GAME_COLOR_BLACK : keys_color, background);
    } else if (row == ROW_SPEED) {
        snprintf(text, sizeof(text), "VELOCIDAD <%u>", speed);
        game_font_draw_text(SETUP_TEXT_X, text_y, text, GAME_UI_TEXT_SCALE, color, background);
    } else {
        game_font_draw_text(SETUP_TEXT_X, text_y, "INICIAR", GAME_UI_TEXT_SCALE, is_selected ? GAME_COLOR_BLACK : start_color, background);
    }
}

static void draw_setup(void) {
    static const char *title = "ESCRITURA";
    char               text[16];

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    game_font_draw_text((GAME_SCREEN_WIDTH - game_font_text_width(title, SETUP_TITLE_SCALE)) / 2, SETUP_TITLE_Y, title, SETUP_TITLE_SCALE, title_color, GAME_COLOR_BLACK);
    for (uint8_t row = 0; row < ROW_COUNT; row++) {
        draw_setup_row(row);
    }
    snprintf(text, sizeof(text), "REC %06lu", (unsigned long)record);
    game_font_draw_text(SETUP_TEXT_X, SETUP_RECORD_Y, text, GAME_UI_TEXT_SCALE, record_color, GAME_COLOR_BLACK);
    game_font_draw_text(SETUP_TEXT_X, SETUP_HINT_Y, "ESC: MENU", GAME_UI_TEXT_SCALE, hint_color, GAME_COLOR_BLACK);
}

// El cursor queda donde estaba: tras una partida sigue en INICIAR y un clic juega otra igual
static void show_setup(void) {
    state = STATE_SETUP;
    draw_setup();
}

// Mueve el cursor una fila, dando la vuelta en los extremos; solo se repintan las dos filas
static void move_cursor(int8_t delta) {
    uint8_t previous = cursor;
    cursor           = (uint8_t)((cursor + ROW_COUNT + delta) % ROW_COUNT);
    draw_setup_row(previous);
    draw_setup_row(cursor);
}

// Marca o desmarca un conjunto; el último marcado no se deja quitar, así siempre hay qué teclear
static void toggle_set(uint8_t index) {
    uint8_t mask = set_mask ^ (1 << index);
    if (mask == 0) {
        return;
    }
    set_mask = mask;
    draw_setup_row(index);
}

static void set_speed(uint8_t value) {
    speed = value;
    draw_setup_row(ROW_SPEED);
}

static void start_game(uint32_t now_ms) {
    // Solo se escribe en la EEPROM si la configuración cambió desde la última vez
    uint32_t preferences = pack_preferences();
    if (preferences != saved_preferences) {
        saved_preferences = preferences;
        game_host_typing_preferences_save(preferences);
    }

    build_alphabet();
    reset_round();
    state       = STATE_PLAYING;
    start_ms    = now_ms;
    step_timer  = now_ms;
    lpm_timer   = now_ms;
    // La primera letra sale enseguida
    spawn_timer = now_ms - spawn_interval();

    game_host_fill_rect(0, 0, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT, GAME_COLOR_BLACK);
    draw_hud_score();
    draw_hud_lives();
    draw_hud_stats();
    game_host_fill_rect(0, HUD_HEIGHT, GAME_SCREEN_WIDTH, SEPARATOR_HEIGHT, separator_color);
    draw_floor(floor_color);
}

void game_typing_init(uint32_t seed, uint32_t now_ms) {
    rng_state  = seed != 0 ? seed : 1;
    record     = game_host_record_load(GAME_ID_TYPING);
    step_timer = now_ms;
    reset_round();
    load_preferences();
    cursor = 0;
    show_setup();
}

// La rueda solo manda izquierda y derecha: fuera de la velocidad también mueven el cursor
// (izquierda sube, derecha baja). En la velocidad la cambian, y al pasarse del extremo sacan el
// cursor de la fila para que con la rueda sola no se quede atrapado ahí.
static void setup_input(game_input_t input, uint32_t now_ms) {
    switch (input) {
        case GAME_INPUT_UP:
            move_cursor(-1);
            break;
        case GAME_INPUT_DOWN:
            move_cursor(1);
            break;
        case GAME_INPUT_LEFT:
            if (cursor == ROW_SPEED && speed > SPEED_MIN) {
                set_speed(speed - 1);
            } else {
                move_cursor(-1);
            }
            break;
        case GAME_INPUT_RIGHT:
            if (cursor == ROW_SPEED && speed < SPEED_MAX) {
                set_speed(speed + 1);
            } else {
                move_cursor(1);
            }
            break;
        case GAME_INPUT_ACTION:
            if (cursor < SET_COUNT) {
                toggle_set(cursor);
            } else if (cursor == ROW_SPEED) {
                set_speed(speed < SPEED_MAX ? speed + 1 : SPEED_MIN);
            } else {
                start_game(now_ms);
            }
            break;
        default:
            break;
    }
}

void game_typing_input(game_input_t input, uint32_t now_ms) {
    switch (state) {
        case STATE_SETUP:
            setup_input(input, now_ms);
            break;
        case STATE_OVER:
            if (input == GAME_INPUT_ACTION) {
                show_setup();
            }
            break;
        default:
            // Mientras se juega no hacen nada: solo cuentan las letras
            break;
    }
}

bool game_typing_wants_text(void) {
    return state == STATE_PLAYING;
}

void game_typing_type(uint8_t character, uint32_t now_ms) {
    // El espacio puede llegar como texto: no es una letra del juego ni cuenta como fallo
    if (state != STATE_PLAYING || character == ' ') {
        return;
    }

    // Se distingue entre mayúsculas y minúsculas: la letra tiene que ser exactamente la tecleada.
    // Se lleva la letra igual que esté más abajo: la más urgente
    letter_t *target = NULL;
    for (uint8_t i = 0; i < MAX_LETTERS; i++) {
        letter_t *letter = &letters[i];
        if (letter->state == LETTER_FALLING && letter->character == character && (target == NULL || letter->drawn_y > target->drawn_y)) {
            target = letter;
        }
    }
    if (target == NULL) {
        // Tecla equivocada: solo baja la precisión
        misses++;
        update_hud_precision();
        return;
    }

    target->state  = LETTER_HIT;
    target->hit_ms = now_ms;
    game_font_draw_glyph(letter_x(target), target->drawn_y, target->character, LETTER_SCALE, hit_color);
    hits++;
    // Cada tramo de velocidad y cada conjunto marcado dan más puntos por letra
    score += (uint32_t)POINTS_PER_HIT * (tier() + 1) * set_count;
    if (score > MAX_SCORE) {
        score = MAX_SCORE;
    }
    draw_hud_score();
    update_hud_precision();
}

void game_typing_tick(uint32_t now_ms) {
    if (state != STATE_PLAYING) {
        return;
    }
    if (now_ms - step_timer < STEP_MS) {
        return;
    }
    step_timer += STEP_MS;
    if (now_ms - step_timer >= MAX_STEP_LAG * STEP_MS) {
        step_timer = now_ms;
    }
    step(now_ms);
}
