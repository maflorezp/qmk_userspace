#!/usr/bin/env bash
# Compila el firmware del Sofle en Docker (imagen oficial de QMK) y deja los .uf2 en firmware/,
# con la versión en el nombre (por ejemplo firmware/sofle-c47b795+ab12-normal.uf2).
# Uso: ./build.sh            -> firmware genérico, sirve para las dos mitades
#      ./build.sh hands      -> además, un .uf2 por mitad que graba el lado en la EEPROM
#                               (solo hace falta la primera vez o después de borrar la EEPROM)
#      ./build.sh pinscan    -> además, firmware de diagnóstico de pines para la mitad derecha
#      ./build.sh i2cscan    -> además, firmware de diagnóstico que busca la OLED en el bus I2C
#      ./build.sh oledtest   -> además, firmware de diagnóstico con todos los píxeles de las OLED encendidos
#      ./build.sh keymap     -> tras flashear, las capas vuelven a las de keymap.c (usar cuando cambian
#                               teclas en el código: si no, el watcher restaura las del respaldo)
#
# El firmware normal queda además en firmware/pending/ (sofle_L-<versión>.uf2 y sofle_R-<versión>.uf2)
# para que tools/flash_watcher.py lo cargue en cada mitad. Con "hands" se encolan las versiones con lado.
set -euo pipefail

USERSPACE_DIR="$(cd "$(dirname "$0")" && pwd)"
QMK_FIRMWARE_DIR="$(realpath "${QMK_FIRMWARE_DIR:-$HOME/qmk_firmware}")"
IMAGE="ghcr.io/qmk/qmk_cli"
KEYBOARD="sofle/rev1"
KEYMAP="maflorezp"
OUTPUT_DIR="$USERSPACE_DIR/firmware"
# Archivos intermedios de la compilación en /tmp (tmpfs, en RAM): no desgasta el disco y es más rápido
BUILD_TMP_DIR="${BUILD_TMP_DIR:-/tmp/qmk-build}"
PENDING_DIR="$OUTPUT_DIR/pending"

mkdir -p "$OUTPUT_DIR" "$PENDING_DIR" "$BUILD_TMP_DIR"

# Versión embebida en un .uf2 (la misma que reporta el teclado por USB)
uf2_version() {
    python3 - "$USERSPACE_DIR/tools" "$1" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import flash_watcher
print(flash_watcher.uf2_version(Path(sys.argv[2])) or "sinversion")
PY
}

# Compila y deja el .uf2 como firmware/<nombre>-<versión>.uf2; su ruta queda en BUILT_FILE
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
        -v "$BUILD_TMP_DIR":/qmk_build:z \
        -e QMK_USERSPACE=/qmk_userspace \
        -e BUILD_DIR=/qmk_build \
        -e SKIP_GIT=1 \
        "$IMAGE" qmk compile -c -kb "$KEYBOARD" -km "$KEYMAP" ${hand:+-e HAND="$hand"} "$@" |
        grep -E "error|warning|Creating UF2" || true
    local built="$USERSPACE_DIR/sofle_rev1_${KEYMAP}${suffix}.uf2"
    if [[ ! -f "$built" ]]; then
        echo "ERROR: $name no compiló; revisa los errores de arriba. No se encoló nada." >&2
        exit 1
    fi
    BUILT_FILE="$OUTPUT_DIR/$name-$(uf2_version "$built").uf2"
    mv -f "$built" "$BUILT_FILE"
}

# Deja un firmware en la cola de una mitad (L o R), reemplazando el que hubiera
queue() {
    local side="$1" file="$2" version
    version="$(basename "$file" .uf2)"
    version="${version#*-}"
    rm -f "$PENDING_DIR/sofle_${side}"*.uf2
    cp "$file" "$PENDING_DIR/sofle_${side}-${version}.uf2"
}

compile sofle ""
generic="$BUILT_FILE"

rm -f "$PENDING_DIR/keymap-reset"
for arg in "$@"; do
    if [[ "$arg" == "keymap" ]]; then
        touch "$PENDING_DIR/keymap-reset"
    fi
done

if [[ "${1:-}" == "hands" ]]; then
    compile sofle_left left
    queue L "$BUILT_FILE"
    compile sofle_right right
    queue R "$BUILT_FILE"
else
    queue L "$generic"
    queue R "$generic"
fi

case "${1:-}" in
    pinscan) compile sofle_right_pinscan right -e PIN_SCAN=yes ;;
    i2cscan) compile sofle_i2cscan "" -e I2C_SCAN=yes ;;
    oledtest) compile sofle_oledtest "" -e OLED_TEST=yes ;;
esac

echo
echo "Compilado: $(basename "$generic")"
echo "En cola:   $(ls "$PENDING_DIR" | tr '\n' ' ')"
