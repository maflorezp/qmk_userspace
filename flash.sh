#!/usr/bin/env bash
# Flashea un .uf2 en la mitad que esté en modo de carga (unidad RPI-RP2).
# Uso: ./flash.sh firmware/sofle_left.uf2
#
# Para poner una mitad en modo de carga:
#   - con el teclado funcionando: LOWER + RAISE y luego Esc (entra la mitad conectada al USB), o
#   - Bootmagic: mantener la tecla de la esquina superior externa de esa mitad mientras se conecta el USB.
# Nunca conectar ni desconectar el cable entre mitades con el USB conectado.
set -euo pipefail

UF2_FILE="${1:?Uso: $0 <archivo.uf2>}"
[[ -f "$UF2_FILE" ]] || { echo "No existe $UF2_FILE" >&2; exit 1; }

echo "Esperando la unidad RPI-RP2..."
device=""
until [[ -n "$device" ]]; do
    device="$(lsblk -rpno NAME,LABEL | awk '$2=="RPI-RP2"{print $1; exit}')"
    [[ -n "$device" ]] || sleep 1
done
echo "Detectada en $device"

mount_point="$(lsblk -rno MOUNTPOINT "$device")"
if [[ -z "$mount_point" ]]; then
    udisksctl mount -b "$device" >/dev/null
    mount_point="$(lsblk -rno MOUNTPOINT "$device")"
fi

echo "Copiando $(basename "$UF2_FILE") a $mount_point"
cp "$UF2_FILE" "$mount_point/"
sync

# Al terminar de recibir el .uf2, el RP2040 se reinicia solo y la unidad desaparece
for _ in $(seq 1 15); do
    lsblk -rpno NAME | grep -qx "$device" || { echo "Listo: la placa se reinició con el firmware nuevo"; exit 0; }
    sleep 1
done
echo "La unidad sigue presente: revisar si la copia terminó bien" >&2
exit 1
