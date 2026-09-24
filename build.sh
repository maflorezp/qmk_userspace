#!/usr/bin/env bash
# Compila el firmware del Sofle en Docker (imagen oficial de QMK) y deja los .uf2 en firmware/.
# Uso: ./build.sh            -> firmware genérico (sofle.uf2), sirve para las dos mitades
#      ./build.sh hands      -> además, un .uf2 por mitad que graba el lado en la EEPROM
#                               (solo hace falta la primera vez o después de borrar la EEPROM)
#      ./build.sh pinscan    -> además, firmware de diagnóstico de pines para la mitad derecha
#      ./build.sh i2cscan    -> además, firmware de diagnóstico que busca la OLED en el bus I2C
#      ./build.sh oledtest   -> además, firmware de diagnóstico con todos los píxeles de las OLED encendidos
#
# El firmware normal queda además en firmware/pending/ (sofle_L.uf2 y sofle_R.uf2) para que
# tools/flash_watcher.py lo cargue en cada mitad. Con "hands" se encolan las versiones con lado.
set -euo pipefail

USERSPACE_DIR="$(cd "$(dirname "$0")" && pwd)"
QMK_FIRMWARE_DIR="$(realpath "${QMK_FIRMWARE_DIR:-$HOME/qmk_firmware}")"
IMAGE="ghcr.io/qmk/qmk_cli"
KEYBOARD="sofle/rev1"
KEYMAP="maflorezp"
OUTPUT_DIR="$USERSPACE_DIR/firmware"

mkdir -p "$OUTPUT_DIR"

compile() {
    local name="$1" hand="$2"
    shift 2
    local suffix=""
    [[ -n "$hand" ]] && suffix="_${hand}"
    echo "==> Compilando $name"
    docker run --rm --user "$(id -u):$(id -g)" \
        -w /qmk_firmware \
        -v "$QMK_FIRMWARE_DIR":/qmk_firmware:z \
        -v "$USERSPACE_DIR":/qmk_userspace:z \
        -e QMK_USERSPACE=/qmk_userspace \
        -e SKIP_GIT=1 \
        "$IMAGE" qmk compile -c -kb "$KEYBOARD" -km "$KEYMAP" ${hand:+-e HAND="$hand"} "$@" |
        grep -E "error|warning|Creating UF2" || true
    mv -f "$USERSPACE_DIR/sofle_rev1_${KEYMAP}${suffix}.uf2" "$OUTPUT_DIR/$name.uf2"
}

compile sofle ""

if [[ "${1:-}" == "hands" ]]; then
    compile sofle_left left
    compile sofle_right right
fi

if [[ "${1:-}" == "pinscan" ]]; then
    compile sofle_right_pinscan right -e PIN_SCAN=yes
fi

if [[ "${1:-}" == "i2cscan" ]]; then
    compile sofle_i2cscan "" -e I2C_SCAN=yes
fi

if [[ "${1:-}" == "oledtest" ]]; then
    compile sofle_oledtest "" -e OLED_TEST=yes
fi

# Encola el firmware para tools/flash_watcher.py
mkdir -p "$OUTPUT_DIR/pending"
if [[ "${1:-}" == "hands" ]]; then
    cp "$OUTPUT_DIR/sofle_left.uf2" "$OUTPUT_DIR/pending/sofle_L.uf2"
    cp "$OUTPUT_DIR/sofle_right.uf2" "$OUTPUT_DIR/pending/sofle_R.uf2"
else
    cp "$OUTPUT_DIR/sofle.uf2" "$OUTPUT_DIR/pending/sofle_L.uf2"
    cp "$OUTPUT_DIR/sofle.uf2" "$OUTPUT_DIR/pending/sofle_R.uf2"
fi

ls -la "$OUTPUT_DIR" "$OUTPUT_DIR/pending"
