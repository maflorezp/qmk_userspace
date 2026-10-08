#!/usr/bin/env bash
# Compila y abre el emulador de la pantalla a color con los juegos del teclado.
# Uso: ./run.sh                -> ventana al doble de tamaño
#      ./run.sh --scale 3      -> ventana al triple
#      ./run.sh --game tetris  -> entra directo a un juego (car o tetris)
#      ./run.sh --script "..." -> prueba sin ventana (ver main.c)
# Controles: flechas o rueda del mouse = flechas y rueda del teclado; espacio o clic = clic;
#            Esc = volver al menú (en el menú, salir); Q = salir
set -euo pipefail

EMULATOR_DIR="$(cd "$(dirname "$0")" && pwd)"
GAMES_DIR="$EMULATOR_DIR/../../keyboards/sofle/keymaps/maflorezp/games"
# El binario va a /tmp (en RAM), igual que los intermedios del firmware
BUILD_TMP_DIR="${BUILD_TMP_DIR:-/tmp/qmk-build}"
BINARY="$BUILD_TMP_DIR/lcd_emulator"

mkdir -p "$BUILD_TMP_DIR"
# shellcheck disable=SC2046
gcc -std=gnu11 -O2 -Wall -Wextra -Werror -I"$GAMES_DIR" \
    "$EMULATOR_DIR/main.c" "$GAMES_DIR"/*.c \
    $(pkg-config --cflags --libs sdl2) -o "$BINARY"
exec "$BINARY" "$@"
