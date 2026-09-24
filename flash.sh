#!/usr/bin/env bash
# Flashea un .uf2 en la mitad que esté en modo de carga (unidad RPI-RP2).
# Uso: ./flash.sh <archivo.uf2>               -> la primera mitad que aparezca
#      ./flash.sh <archivo.uf2> left|right    -> solo esa mitad; si aparece la otra, no toca nada
#      ./flash.sh <archivo.uf2> both          -> las dos, una tras otra, sin repetir la misma
#
# Cada mitad se reconoce por el punto donde el sistema monta su unidad (fstab por UUID del Pico).
# Se puede cambiar con LEFT_MOUNT_PATTERN / RIGHT_MOUNT_PATTERN.
#
# Para poner una mitad en modo de carga (siempre entra la que tiene el USB):
#   - doble toque rápido (< 0,5 s) al botón de reset, o
#   - LOWER + RAISE y luego Esc (la tira queda en rojo y la OLED dice CARGA), o
#   - Bootmagic: mantener la tecla de la esquina superior externa mientras se conecta el USB.
# Nunca conectar ni desconectar el cable entre mitades con el USB conectado.
set -euo pipefail

UF2_FILE="${1:?Uso: $0 <archivo.uf2> [left|right|both]}"
TARGET="${2:-any}"
LEFT_MOUNT_PATTERN="${LEFT_MOUNT_PATTERN:-PicoL}"
RIGHT_MOUNT_PATTERN="${RIGHT_MOUNT_PATTERN:-PicoR}"

[[ -f "$UF2_FILE" ]] || { echo "No existe $UF2_FILE" >&2; exit 1; }
[[ "$TARGET" =~ ^(any|left|right|both)$ ]] || { echo "Mitad inválida: $TARGET (left, right o both)" >&2; exit 1; }

side_of() {
    case "$1" in
        *"$LEFT_MOUNT_PATTERN"*) echo left ;;
        *"$RIGHT_MOUNT_PATTERN"*) echo right ;;
        *) echo unknown ;;
    esac
}

side_name() {
    case "$1" in
        left) echo "izquierda" ;;
        right) echo "derecha" ;;
        *) echo "desconocida" ;;
    esac
}

# Espera una mitad en modo de carga y la flashea si es la esperada ("any" acepta cualquiera).
# Deja en FLASHED_SIDE la mitad que se flasheó.
flash_one() {
    local expected="$1" device="" mount_point side
    echo "Esperando una mitad en modo de carga ($(side_name "$expected" | sed 's/desconocida/cualquiera/'))..."
    while true; do
        device="$(lsblk -rpno NAME,LABEL | awk '$2=="RPI-RP2"{print $1; exit}')"
        [[ -n "$device" ]] && break
        sleep 1
    done

    mount_point="$(lsblk -rno MOUNTPOINT "$device")"
    if [[ -z "$mount_point" ]]; then
        udisksctl mount -b "$device" >/dev/null
        mount_point="$(lsblk -rno MOUNTPOINT "$device")"
    fi
    side="$(side_of "$mount_point")"

    if [[ "$expected" != "any" && "$side" != "$expected" ]]; then
        udisksctl unmount -b "$device" >/dev/null || true
        echo "Entró la mitad $(side_name "$side") ($mount_point) y se esperaba la $(side_name "$expected"): no se flasheó nada." >&2
        echo "Desconéctala, pasa el USB a la mitad $(side_name "$expected") y vuelve a intentarlo." >&2
        return 1
    fi

    echo "Mitad $(side_name "$side") en $mount_point: copiando $(basename "$UF2_FILE")"
    cp "$UF2_FILE" "$mount_point/"
    sync

    # Al terminar de recibir el .uf2, el RP2040 se reinicia solo y la unidad desaparece
    for _ in $(seq 1 15); do
        if ! lsblk -rpno NAME | grep -qx "$device"; then
            echo "Listo: la mitad $(side_name "$side") se reinició con el firmware nuevo"
            FLASHED_SIDE="$side"
            return 0
        fi
        sleep 1
    done
    echo "La unidad sigue presente: revisar si la copia terminó bien" >&2
    return 1
}

if [[ "$TARGET" == "both" ]]; then
    flash_one any
    first="$FLASHED_SIDE"
    [[ "$first" == "unknown" ]] && { echo "No reconocí la mitad; flashea la otra con left/right" >&2; exit 1; }
    other="left"
    [[ "$first" == "left" ]] && other="right"
    echo
    echo ">>> Ahora pasa el USB a la mitad $(side_name "$other") y ponla en modo de carga."
    until flash_one "$other"; do
        echo "Reintentando..."
        sleep 2
    done
    echo
    echo "Las dos mitades tienen $(basename "$UF2_FILE"). Devuelve el USB a la mitad de siempre."
else
    flash_one "$TARGET"
fi
