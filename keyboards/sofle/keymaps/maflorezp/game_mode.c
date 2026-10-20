#include "maflorezp.h"
#include "game_system.h"
#include "status_screen.h"

// Modo juego: CK_GAME abre el menú de juegos (games/game_system.c) en la pantalla a color. Mientras
// dura, ninguna tecla llega al PC: todas se traducen a entradas del juego por lo que valen en la capa
// activa.
//   A / J izquierda · D / L derecha · W / I arriba · S / K abajo, o las flechas de LOWER
//   Espacio, Enter o clic de la rueda: acción · Esc: volver al menú y, desde el menú, salir
//   Girar la rueda: izquierda o derecha
// La práctica de escritura recibe el texto que escribiría el PC (ver typed_character).
// LOWER y RAISE siguen funcionando, para usar las flechas de LOWER. Las teclas que se sueltan
// también siguen su camino normal: así nada queda pegado al entrar o salir.

// Clic de la rueda derecha en la matriz
#define WHEEL_CLICK_ROW 9
#define WHEEL_CLICK_COL 5

// Las preferencias de la práctica de escritura van en el último espacio de récords
#define TYPING_PREFERENCES_SLOT (GAME_RECORD_COUNT - 1)

_Static_assert(GAME_ID_COUNT <= TYPING_PREFERENCES_SLOT, "No hay espacio en la EEPROM para el récord de cada juego");

static bool active = false;

// Estado del teclado para la práctica de escritura (ver typed_character)
typedef enum {
    DEAD_NONE,
    DEAD_ACUTE,
    DEAD_DIAERESIS,
} dead_key_t;

static bool       shift_held   = false;
static bool       altgr_held   = false;
static dead_key_t pending_dead = DEAD_NONE;

bool game_mode_active(void) {
    return active;
}

void game_mode_exit(void) {
    if (!active) {
        return;
    }
    active       = false;
    shift_held   = false;
    altgr_held   = false;
    pending_dead = DEAD_NONE;
    layer_clear();
    // La pantalla de estado se redibuja completa sobre lo que dejó el juego
    status_screen_reset();
}

void game_mode_toggle(void) {
    if (active) {
        game_mode_exit();
        return;
    }
    // Los juegos viven en la pantalla a color de la mitad del USB
    if (!lcd_is_active()) {
        display_show_overlay("Pantalla apagada");
        return;
    }
    active = true;
    uint32_t now = timer_read32();
    // El momento exacto en que se entra hace distinta cada sesión
    game_system_init(now ^ 0x9E3779B9, now);
}

// --- Texto para la práctica de escritura ---
// Las teclas no llegan al PC, así que aquí se interpretan como lo haría su distribución, US
// internacional con AltGr (altgr-intl): Shift da mayúsculas y los símbolos de arriba; AltGr + vocal
// da la vocal con tilde, AltGr + n la ñ, AltGr + y la ü, AltGr + 1 la ¡ y AltGr + / la ¿; AltGr + '
// es la tilde muerta y AltGr + Shift + ' la diéresis muerta, que se combinan con la tecla siguiente.
// Los caracteres salen en Latin-1, que es lo que dibuja la fuente.

// Símbolos de la distribución US: sin Shift y con Shift, para las teclas que no son letras
static const struct {
    uint8_t keycode;
    char    plain;
    char    shifted;
} us_symbols[] = {
    {KC_1, '1', '!'}, {KC_2, '2', '@'}, {KC_3, '3', '#'}, {KC_4, '4', '$'}, {KC_5, '5', '%'},
    {KC_6, '6', '^'}, {KC_7, '7', '&'}, {KC_8, '8', '*'}, {KC_9, '9', '('}, {KC_0, '0', ')'},
    {KC_MINS, '-', '_'}, {KC_EQL, '=', '+'}, {KC_LBRC, '[', '{'}, {KC_RBRC, ']', '}'}, {KC_BSLS, '\\', '|'},
    {KC_SCLN, ';', ':'}, {KC_QUOT, '\'', '"'}, {KC_GRV, '`', '~'}, {KC_COMM, ',', '<'}, {KC_DOT, '.', '>'},
    {KC_SLSH, '/', '?'},
};

// Vocales con tilde (á é í ó ú) y con diéresis (ü), en minúscula; la mayúscula está 0x20 más abajo
static uint8_t accented(uint8_t vowel, dead_key_t dead) {
    if (dead == DEAD_DIAERESIS) {
        return vowel == 'u' ? 0xFC : 0;
    }
    switch (vowel) {
        case 'a':
            return 0xE1;
        case 'e':
            return 0xE9;
        case 'i':
            return 0xED;
        case 'o':
            return 0xF3;
        case 'u':
            return 0xFA;
        default:
            return 0;
    }
}

static uint8_t with_case(uint8_t lower, bool upper) {
    return upper ? lower - 0x20 : lower;
}

// Carácter que escribe la tecla con los modificadores del momento; 0 si no escribe nada (incluye
// las teclas muertas, que quedan pendientes para la siguiente)
static uint8_t typed_character(uint16_t keycode) {
    bool shift = shift_held;
    bool altgr = altgr_held;
    // Teclas de las capas que ya traen modificadores, como KC_LPRN = Shift + 9
    if (IS_QK_MODS(keycode)) {
        uint8_t mods = QK_MODS_GET_MODS(keycode);
        shift |= (mods & MOD_MASK_SHIFT) != 0;
        altgr |= (mods & MOD_RALT) == MOD_RALT;
        keycode = QK_MODS_GET_BASIC_KEYCODE(keycode);
    }

    if (altgr && keycode == KC_QUOT) {
        pending_dead = shift ? DEAD_DIAERESIS : DEAD_ACUTE;
        return 0;
    }
    dead_key_t dead = pending_dead;
    pending_dead    = DEAD_NONE;

    if (keycode >= KC_A && keycode <= KC_Z) {
        uint8_t letter = 'a' + (keycode - KC_A);
        if (altgr) {
            // AltGr + letra: las que tienen tilde, ñ y ü
            uint8_t special = letter == 'n' ? 0xF1 : letter == 'y' ? 0xFC : accented(letter, DEAD_ACUTE);
            return special != 0 ? with_case(special, shift) : 0;
        }
        if (dead != DEAD_NONE && accented(letter, dead) != 0) {
            return with_case(accented(letter, dead), shift);
        }
        return with_case(letter, shift);
    }
    if (altgr) {
        if (keycode == KC_1) {
            return 0xA1; // ¡
        }
        return keycode == KC_SLSH ? 0xBF : 0; // ¿
    }
    for (uint8_t i = 0; i < ARRAY_SIZE(us_symbols); i++) {
        if (us_symbols[i].keycode == keycode) {
            return shift ? us_symbols[i].shifted : us_symbols[i].plain;
        }
    }
    return 0;
}

// Shift y AltGr no llegan al PC: se recuerdan aquí. Devuelve true si la tecla era uno de ellos.
static bool track_modifier(uint16_t keycode, bool pressed) {
    switch (keycode) {
        case KC_LSFT:
        case KC_RSFT:
            shift_held = pressed;
            return true;
        case KC_RALT:
            altgr_held = pressed;
            return true;
        default:
            return false;
    }
}

// Traduce una tecla a una entrada del juego; devuelve false si la tecla no tiene uso en los juegos
static bool translate(uint16_t keycode, keyrecord_t *record, game_input_t *input) {
    if (IS_ENCODEREVENT(record->event)) {
        *input = record->event.type == ENCODER_CW_EVENT ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
        return true;
    }
    if (record->event.key.row == WHEEL_CLICK_ROW && record->event.key.col == WHEEL_CLICK_COL) {
        *input = GAME_INPUT_ACTION;
        return true;
    }
    switch (keycode) {
        case KC_A:
        case KC_J:
        case KC_LEFT:
            *input = GAME_INPUT_LEFT;
            return true;
        case KC_D:
        case KC_L:
        case KC_RGHT:
            *input = GAME_INPUT_RIGHT;
            return true;
        case KC_W:
        case KC_I:
        case KC_UP:
            *input = GAME_INPUT_UP;
            return true;
        case KC_S:
        case KC_K:
        case KC_DOWN:
            *input = GAME_INPUT_DOWN;
            return true;
        case KC_SPC:
        case KC_ENT:
            *input = GAME_INPUT_ACTION;
            return true;
        case KC_ESC:
            *input = GAME_INPUT_BACK;
            return true;
        default:
            return false;
    }
}

bool game_mode_process_record(uint16_t keycode, keyrecord_t *record) {
    if (!active) {
        return true;
    }
    // Las capas y la propia tecla de juegos (para salir con ella) siguen su camino normal
    if (keycode == TL_LOWR || keycode == TL_UPPR || keycode == CK_GAME) {
        return true;
    }
    if (track_modifier(keycode, record->event.pressed)) {
        return false;
    }
    // Los giros de la rueda llegan como presión y suelta: se atiende solo la presión
    if (!record->event.pressed) {
        return !IS_ENCODEREVENT(record->event);
    }
    uint32_t now = timer_read32();
    // En la práctica de escritura, lo que escribe algo es texto; Espacio, Enter, Esc, las flechas
    // y la rueda siguen siendo entradas del juego
    if (game_system_wants_text() && !IS_ENCODEREVENT(record->event) && keycode != KC_SPC && keycode != KC_ENT) {
        uint8_t character = typed_character(keycode);
        if (character != 0) {
            game_system_type(character, now);
            return false;
        }
    }
    game_input_t input;
    if (translate(keycode, record, &input) && !game_system_input(input, now)) {
        // Esc en el menú: se sale del modo juego
        game_mode_exit();
    }
    return false;
}

void game_mode_task(void) {
    if (active) {
        game_system_tick(timer_read32());
    }
}

uint32_t game_host_record_load(game_id_t game) {
    return settings_game_record(game);
}

void game_host_record_save(game_id_t game, uint32_t record) {
    settings_save_game_record(game, record);
}

uint32_t game_host_typing_preferences_load(void) {
    return settings_game_record(TYPING_PREFERENCES_SLOT);
}

void game_host_typing_preferences_save(uint32_t preferences) {
    settings_save_game_record(TYPING_PREFERENCES_SLOT, preferences);
}
