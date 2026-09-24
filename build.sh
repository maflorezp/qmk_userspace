#!/usr/bin/env bash
# Compila el firmware del Sofle en Docker (imagen oficial de QMK) y deja los .uf2 en firmware/.
# Uso: ./build.sh            -> firmware normal para las dos mitades
#      ./build.sh pinscan    -> además, firmware de diagnóstico de pines para la mitad derecha
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
    echo "==> Compilando $name"
    docker run --rm --user "$(id -u):$(id -g)" \
        -w /qmk_firmware \
        -v "$QMK_FIRMWARE_DIR":/qmk_firmware:z \
        -v "$USERSPACE_DIR":/qmk_userspace:z \
        -e QMK_USERSPACE=/qmk_userspace \
        -e SKIP_GIT=1 \
        "$IMAGE" qmk compile -c -kb "$KEYBOARD" -km "$KEYMAP" -e HAND="$hand" "$@" |
        grep -E "error|warning|Creating UF2" || true
    mv -f "$USERSPACE_DIR/sofle_rev1_${KEYMAP}_${hand}.uf2" "$OUTPUT_DIR/$name.uf2"
}

compile sofle_left left
compile sofle_right right

if [[ "${1:-}" == "pinscan" ]]; then
    compile sofle_right_pinscan right -e PIN_SCAN=yes
fi

ls -la "$OUTPUT_DIR"
