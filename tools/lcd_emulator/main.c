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
//   Pasos: left, right, up, down, action, back, wait:<ms>, shot:<archivo.ppm>
//   --game car|tetris entra directo al juego; sin eso se abre el menú

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game_host.h"
#include "game_system.h"

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

static int run_window(int scale, uint32_t seed, int game) {
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

    start(seed, game, SDL_GetTicks());

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            uint32_t     now       = SDL_GetTicks();
            bool         has_input = false;
            game_input_t input     = GAME_INPUT_ACTION;

            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0) {
                input     = event.wheel.y > 0 ? GAME_INPUT_RIGHT : GAME_INPUT_LEFT;
                has_input = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                has_input = true;
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

        game_system_tick(SDL_GetTicks());

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
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script = argv[++i];
            // Sin semilla explícita, las pruebas automáticas deben repetirse igual
            seed = DEFAULT_SEED;
        } else {
            fprintf(stderr, "Uso: %s [--scale N] [--seed N] [--game car|tetris] [--script pasos]\n", argv[0]);
            return 1;
        }
    }
    if (scale < 1) {
        scale = DEFAULT_SCALE;
    }
    return script != NULL ? run_script(script, seed, game) : run_window(scale, seed, game);
}
