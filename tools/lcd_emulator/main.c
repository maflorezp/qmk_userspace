// Emulador de la pantalla a color del Sofle (172x320) para probar los juegos en el PC.
// Corre el mismo código de keyboards/sofle/keymaps/maflorezp/games que irá al teclado: aquí solo
// se implementa game_host.h sobre una ventana de SDL2.
//
// Controles: flechas o rueda del mouse = flechas y rueda del teclado
//            espacio, Enter o clic del mouse = clic de la rueda
//            Esc = volver al menú (en el menú, salir); Q = salir
//
// Modo sin ventana, para pruebas automáticas (el tiempo es simulado):
//   lcd_emulator --game tetris --script "action,wait:500,left,wait:1000,shot:/tmp/captura.ppm"
//   Pasos: left, right, up, down, action, back, type:<texto UTF-8>, wait:<ms>, shot:<archivo.ppm>
//   --game car|tetris|breakout|snake|typing entra directo al juego; sin eso se abre el menú
//
// Pantalla de estado (--status), con datos inventados que se cambian con el teclado del PC:
//   L capa siguiente · C Caps Lock · N Num Lock · 1-4 Win/Ctrl/Alt/Shift · R grabar macro
//   M mensaje · W sube la velocidad de escritura · V versión distinta · U USB · H hora del PC o sin hora
//   Con --script los pasos son key:<letra>, wait:<ms> y shot:<archivo.ppm>

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game_host.h"
#include "game_system.h"
#include "status_screen.h"

#define DEFAULT_SCALE 2
#define FRAME_DELAY_MS 5
#define SCRIPT_TICK_MS 5
#define DEFAULT_SEED 12345

static uint32_t framebuffer[GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT];
static uint32_t records[GAME_ID_COUNT];
// Píxeles y rectángulos pintados: estiman cuánto viajaría por SPI en el teclado
static uint32_t painted_pixels;
static uint32_t painted_rects;

// Misma conversión entera de HSV a RGB que usa QMK (región de 43 pasos de tono)
static uint32_t hsv_to_argb(game_color_t color) {
    uint8_t r, g, b;
    if (color.s == 0) {
        r = g = b = color.v;
    } else {
        uint8_t region    = color.h / 43;
        uint8_t remainder = (color.h - region * 43) * 6;
        uint8_t p         = (color.v * (255 - color.s)) >> 8;
        uint8_t q         = (color.v * (255 - ((color.s * remainder) >> 8))) >> 8;
        uint8_t t         = (color.v * (255 - ((color.s * (255 - remainder)) >> 8))) >> 8;
        switch (region) {
            case 0: r = color.v; g = t; b = p; break;
            case 1: r = q; g = color.v; b = p; break;
            case 2: r = p; g = color.v; b = t; break;
            case 3: r = p; g = q; b = color.v; break;
            case 4: r = t; g = p; b = color.v; break;
            default: r = color.v; g = p; b = q; break;
        }
    }
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void game_host_fill_rect(int16_t x, int16_t y, int16_t width, int16_t height, game_color_t color) {
    int32_t left   = x < 0 ? 0 : x;
    int32_t top    = y < 0 ? 0 : y;
    int32_t right  = x + width > GAME_SCREEN_WIDTH ? GAME_SCREEN_WIDTH : x + width;
    int32_t bottom = y + height > GAME_SCREEN_HEIGHT ? GAME_SCREEN_HEIGHT : y + height;
    if (left >= right || top >= bottom) {
        return;
    }

    uint32_t argb = hsv_to_argb(color);
    for (int32_t row = top; row < bottom; row++) {
        for (int32_t col = left; col < right; col++) {
            framebuffer[row * GAME_SCREEN_WIDTH + col] = argb;
        }
    }
    painted_pixels += (uint32_t)((right - left) * (bottom - top));
    painted_rects++;
}

uint32_t game_host_record_load(game_id_t game) {
    return records[game];
}

void game_host_record_save(game_id_t game, uint32_t record) {
    records[game] = record;
}

// Sin EEPROM: las preferencias duran lo que dura el emulador
static uint32_t typing_preferences;

uint32_t game_host_typing_preferences_load(void) {
    return typing_preferences;
}

void game_host_typing_preferences_save(uint32_t preferences) {
    typing_preferences = preferences;
}

static bool save_ppm(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    fprintf(file, "P6\n%d %d\n255\n", GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT);
    for (size_t i = 0; i < GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT; i++) {
        uint8_t rgb[3] = {(uint8_t)(framebuffer[i] >> 16), (uint8_t)(framebuffer[i] >> 8), (uint8_t)framebuffer[i]};
        fwrite(rgb, 1, sizeof(rgb), file);
    }
    fclose(file);
    return true;
}

static bool parse_input(const char *name, game_input_t *input) {
    static const struct {
        const char  *name;
        game_input_t input;
    } inputs[] = {
        {"left", GAME_INPUT_LEFT}, {"right", GAME_INPUT_RIGHT}, {"up", GAME_INPUT_UP}, {"down", GAME_INPUT_DOWN}, {"action", GAME_INPUT_ACTION}, {"back", GAME_INPUT_BACK},
    };
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        if (strcmp(name, inputs[i].name) == 0) {
            *input = inputs[i].input;
            return true;
        }
    }
    return false;
}

// Toma un carácter UTF-8 y lo devuelve en Latin-1, que es lo que reciben los juegos; lo que no
// existe en Latin-1 queda como '?'
static uint8_t next_latin1(const char **text) {
    const uint8_t *bytes = (const uint8_t *)*text;
    if (bytes[0] < 0x80) {
        *text += 1;
        return bytes[0];
    }
    if ((bytes[0] & 0xE0) == 0xC0 && bytes[1] != 0) {
        *text += 2;
        uint16_t code = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
        return code <= 0xFF ? (uint8_t)code : '?';
    }
    // Secuencias más largas: se saltan los bytes de continuación
    *text += 1;
    while ((**text & 0xC0) == 0x80) {
        *text += 1;
    }
    return '?';
}

// --- Pantalla de estado con datos inventados ---

static const struct {
    const char  *name;
    game_color_t color;
} demo_layers[] = {
    {"QWERTY", {85, 255, 255}}, {"LOWER", {191, 255, 255}}, {"RAISE", {0, 255, 255}},
    {"NUMERIC", {43, 255, 255}}, {"ADJUST", {213, 255, 255}}, {"RGB", {128, 255, 255}},
};
#define DEMO_LAYER_COUNT (sizeof(demo_layers) / sizeof(demo_layers[0]))
#define DEMO_BLINK_MS 500

static status_state_t demo_state;
static unsigned       demo_layer;
static bool           demo_real_clock = true;

static void demo_init(void) {
    memset(&demo_state, 0, sizeof(demo_state));
    demo_state.usb = true;
    snprintf(demo_state.version, sizeof(demo_state.version), "23a9f95+7346-normal");
    demo_state.layer_name  = demo_layers[0].name;
    demo_state.layer_color = demo_layers[0].color;
    status_screen_reset();
}

static void demo_key(char key) {
    switch (key) {
        case 'l':
            demo_layer             = (demo_layer + 1) % DEMO_LAYER_COUNT;
            demo_state.layer_name  = demo_layers[demo_layer].name;
            demo_state.layer_color = demo_layers[demo_layer].color;
            break;
        case 'c': demo_state.caps_lock = !demo_state.caps_lock; break;
        case 'n': demo_state.num_lock = !demo_state.num_lock; break;
        case '1': demo_state.mods ^= STATUS_MOD_GUI; break;
        case '2': demo_state.mods ^= STATUS_MOD_CTRL; break;
        case '3': demo_state.mods ^= STATUS_MOD_ALT; break;
        case '4': demo_state.mods ^= STATUS_MOD_SHIFT; break;
        case 'r': demo_state.recording = !demo_state.recording; break;
        case 'w': demo_state.wpm = (uint8_t)(demo_state.wpm + 17); break;
        case 'v': demo_state.version_warning = !demo_state.version_warning; break;
        case 'u': demo_state.usb = !demo_state.usb; break;
        case 'h': demo_real_clock = !demo_real_clock; break;
        case 'm':
            if (demo_state.message[0] == '\0') {
                snprintf(demo_state.message, sizeof(demo_state.message), "Reunion en 5 minutos sala 3");
            } else {
                demo_state.message[0] = '\0';
            }
            break;
    }
}

// Hora del PC (o fija en las pruebas sin ventana, para que las capturas se repitan) y parpadeos
static void demo_tick(uint32_t now, bool fixed_clock) {
    demo_state.clock_set = demo_real_clock;
    if (fixed_clock) {
        demo_state.hour = 9, demo_state.minute = 41, demo_state.day = 20, demo_state.month = 10;
        demo_state.weekday = status_weekday(2026, 10, 20);
    } else {
        time_t     seconds = time(NULL);
        struct tm *local   = localtime(&seconds);
        demo_state.hour    = (uint8_t)local->tm_hour;
        demo_state.minute  = (uint8_t)local->tm_min;
        demo_state.day     = (uint8_t)local->tm_mday;
        demo_state.month   = (uint8_t)(local->tm_mon + 1);
        demo_state.weekday = status_weekday((uint16_t)(local->tm_year + 1900), demo_state.month, demo_state.day);
    }
    demo_state.colon_visible = (now / DEMO_BLINK_MS) % 2 == 0;
    demo_state.rec_visible   = demo_state.colon_visible;
    status_screen_draw(&demo_state);
}

static int run_status_script(const char *script) {
    uint32_t now   = 0;
    char    *steps = strdup(script);
    demo_init();
    demo_tick(now, true);
    for (char *step = strtok(steps, ","); step != NULL; step = strtok(NULL, ",")) {
        if (strncmp(step, "key:", 4) == 0) {
            demo_key(step[4]);
            demo_tick(now, true);
        } else if (strncmp(step, "wait:", 5) == 0) {
            uint32_t end = now + (uint32_t)atoi(step + 5);
            while (now < end) {
                now += SCRIPT_TICK_MS;
                painted_pixels = 0;
                demo_tick(now, true);
            }
        } else if (strncmp(step, "shot:", 5) == 0) {
            if (!save_ppm(step + 5)) {
                fprintf(stderr, "No se pudo escribir %s\n", step + 5);
                free(steps);
                return 1;
            }
        } else {
            fprintf(stderr, "Paso desconocido: %s\n", step);
            free(steps);
            return 1;
        }
    }
    free(steps);
    return 0;
}

// Abre el menú o, si se pidió un juego, entra directo a él
static void start(uint32_t seed, int game, uint32_t now) {
    game_system_init(seed, now);
    if (game >= 0) {
        game_system_start((game_id_t)game, now);
    }
}

// Ejecuta los pasos con tiempo simulado; informa el máximo pintado entre dos avances del reloj
static int run_script(const char *script, uint32_t seed, int game) {
    uint32_t now        = 0;
    uint32_t max_pixels = 0;
    uint32_t max_rects  = 0;
    char    *steps      = strdup(script);

    start(seed, game, now);
    painted_pixels = painted_rects = 0;

    for (char *step = strtok(steps, ","); step != NULL; step = strtok(NULL, ",")) {
        game_input_t input;
        if (strncmp(step, "wait:", 5) == 0) {
            uint32_t end = now + (uint32_t)atoi(step + 5);
            while (now < end) {
                now += SCRIPT_TICK_MS;
                game_system_tick(now);
                if (painted_pixels > max_pixels) {
                    max_pixels = painted_pixels;
                    max_rects  = painted_rects;
                }
                painted_pixels = painted_rects = 0;
            }
        } else if (strncmp(step, "shot:", 5) == 0) {
            if (!save_ppm(step + 5)) {
                fprintf(stderr, "No se pudo escribir %s\n", step + 5);
                free(steps);
                return 1;
            }
        } else if (strncmp(step, "type:", 5) == 0) {
            // Cada carácter es una tecla; los juegos que no reciben texto la ignoran
            for (const char *text = step + 5; *text != '\0';) {
                game_system_type(next_latin1(&text), now);
            }
            painted_pixels = painted_rects = 0;
        } else if (parse_input(step, &input)) {
            game_system_input(input, now);
            // Lo que pinta una entrada (por ejemplo, el tablero completo al empezar) no cuenta
            // como paso del juego
            painted_pixels = painted_rects = 0;
        } else {
            fprintf(stderr, "Paso desconocido: %s\n", step);
            free(steps);
            return 1;
        }
    }
    free(steps);
    printf("Tiempo simulado: %u ms. Máximo pintado en un paso del juego: %u px en %u rectángulos\n", now, max_pixels, max_rects);
    return 0;
}

static int run_window(int scale, uint32_t seed, int game, bool status) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "No se pudo iniciar SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window   *window   = SDL_CreateWindow("Sofle LCD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, GAME_SCREEN_WIDTH * scale, GAME_SCREEN_HEIGHT * scale, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (window == NULL || renderer == NULL) {
        fprintf(stderr, "No se pudo abrir la ventana: %s\n", SDL_GetError());
        return 1;
    }
    // Píxeles nítidos al ampliar
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GAME_SCREEN_WIDTH, GAME_SCREEN_HEIGHT);

    if (status) {
        demo_init();
    } else {
        start(seed, game, SDL_GetTicks());
    }
    SDL_StartTextInput();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            uint32_t     now       = SDL_GetTicks();
            bool         has_input = false;
            game_input_t input     = GAME_INPUT_ACTION;

            if (event.type == SDL_QUIT) {
                running = false;
            } else if (status) {
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    SDL_Keycode key = event.key.keysym.sym;
                    if (key == SDLK_q || key == SDLK_ESCAPE) {
                        running = false;
                    } else if (key < 128) {
                        demo_key((char)key);
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0) {
                input     = event.wheel.y > 0 ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
                has_input = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                has_input = true;
            } else if (event.type == SDL_TEXTINPUT) {
                // Texto ya interpretado por el PC (Shift, AltGr, tildes): para la práctica de escritura
                for (const char *text = event.text.text; *text != '\0';) {
                    game_system_type(next_latin1(&text), now);
                }
            } else if (event.type == SDL_KEYDOWN && game_system_wants_text() && event.key.keysym.sym != SDLK_ESCAPE && event.key.keysym.sym != SDLK_SPACE && event.key.keysym.sym != SDLK_RETURN) {
                // Mientras el juego recibe texto, las teclas solo llegan como texto (Q no sale)
            } else if (event.type == SDL_KEYDOWN) {
                // Las flechas se repiten al mantenerlas; las demás teclas no
                bool repeat = event.key.repeat != 0;
                has_input   = true;
                switch (event.key.keysym.sym) {
                    case SDLK_q:
                        running   = false;
                        has_input = false;
                        break;
                    case SDLK_LEFT:
                        input = GAME_INPUT_LEFT;
                        break;
                    case SDLK_RIGHT:
                        input = GAME_INPUT_RIGHT;
                        break;
                    case SDLK_UP:
                        input     = GAME_INPUT_UP;
                        has_input = !repeat;
                        break;
                    case SDLK_DOWN:
                        input = GAME_INPUT_DOWN;
                        break;
                    case SDLK_SPACE:
                    case SDLK_RETURN:
                        has_input = !repeat;
                        break;
                    case SDLK_ESCAPE:
                        input     = GAME_INPUT_BACK;
                        has_input = !repeat;
                        break;
                    default:
                        has_input = false;
                        break;
                }
            }
            if (has_input && !game_system_input(input, now)) {
                running = false;
            }
        }

        if (status) {
            demo_tick(SDL_GetTicks(), false);
        } else {
            game_system_tick(SDL_GetTicks());
        }

        SDL_UpdateTexture(texture, NULL, framebuffer, GAME_SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(FRAME_DELAY_MS);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int main(int argc, char **argv) {
    int         scale  = DEFAULT_SCALE;
    uint32_t    seed   = (uint32_t)time(NULL);
    const char *script = NULL;
    // -1: abrir el menú
    int         game   = -1;
    bool        status = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            scale = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc && strcmp(argv[i + 1], "car") == 0) {
            game = GAME_ID_CAR;
            i++;
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc && strcmp(argv[i + 1], "tetris") == 0) {
            game = GAME_ID_TETRIS;
            i++;
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc && strcmp(argv[i + 1], "breakout") == 0) {
            game = GAME_ID_BREAKOUT;
            i++;
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc && strcmp(argv[i + 1], "snake") == 0) {
            game = GAME_ID_SNAKE;
            i++;
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc && strcmp(argv[i + 1], "typing") == 0) {
            game = GAME_ID_TYPING;
            i++;
        } else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script = argv[++i];
            // Sin semilla explícita, las pruebas automáticas deben repetirse igual
            seed = DEFAULT_SEED;
        } else {
            fprintf(stderr, "Uso: %s [--scale N] [--seed N] [--game car|tetris|breakout|snake|typing] [--status] [--script pasos]\n", argv[0]);
            return 1;
        }
    }
    if (scale < 1) {
        scale = DEFAULT_SCALE;
    }
    if (status && script != NULL) {
        return run_status_script(script);
    }
    return script != NULL ? run_script(script, seed, game) : run_window(scale, seed, game, status);
}
