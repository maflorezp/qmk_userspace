#!/usr/bin/env python3
"""Vigila firmware/pending/ y flashea cada mitad del Sofle cuando entra en modo de carga.

Uso: tools/flash_watcher.py        (dejarlo abierto en una terminal; Ctrl+C para salir)

- firmware/pending/sofle_L-<versión>.uf2 va a la mitad izquierda y sofle_R-<versión>.uf2 a la derecha.
- Cada mitad se reconoce por el punto donde el sistema monta su unidad RPI-RP2
  (PicoL / PicoR; se cambia con LEFT_MOUNT_PATTERN / RIGHT_MOUNT_PATTERN).
- Tras copiar, espera a que la mitad arranque y compara el número de serie USB (lleva la
  versión del firmware) con la versión que trae el .uf2. Solo si coinciden lo da por bueno
  y mueve el archivo a firmware/flashed/.
- Mientras hay firmware pendiente y el teclado funciona, guarda un respaldo de lo que tiene
  (capas, macros, luces y configuración) en /tmp; tras flashear y verificar esa mitad, lo
  restaura y lo borra.
- Mensajes en pantalla, notificaciones de escritorio (dunst) y log en /tmp/sofle-flash.log.
"""
import datetime
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sofle_hid  # noqa: E402

REPO_DIR = Path(__file__).resolve().parent.parent
PENDING_DIR = REPO_DIR / "firmware" / "pending"
FLASHED_DIR = REPO_DIR / "firmware" / "flashed"
LOG_FILE = Path("/tmp/sofle-flash.log")

PENDING_PREFIXES = {"left": "sofle_L", "right": "sofle_R"}
SIDE_NAMES = {"left": "izquierda", "right": "derecha", None: "desconocida"}
MOUNT_PATTERNS = {
    "left": os.environ.get("LEFT_MOUNT_PATTERN", "PicoL"),
    "right": os.environ.get("RIGHT_MOUNT_PATTERN", "PicoR"),
}

KEYBOARD_VID, KEYBOARD_PID = "fc32", "0287"
SERIAL_PREFIX = "maflorezp-"
POLL_SECONDS = 1
BACKUP_REFRESH_SECONDS = 30
SETTLE_SECONDS = 3  # espera tras el reinicio, a que el teclado termine de arrancar
USB_SIDE_NAMES = {"izquierda": "left", "derecha": "right"}
REBOOT_TIMEOUT = 15
ENUMERATE_TIMEOUT = 20

COLORS = {"info": "\033[36m", "ok": "\033[32m", "warn": "\033[33m", "error": "\033[31m", "wait": "\033[90m"}
RESET = "\033[0m"


def log(level, message, notify=False):
    stamp = datetime.datetime.now().strftime("%H:%M:%S")
    print(f"{COLORS.get(level, '')}{stamp} {message}{RESET}", flush=True)
    with LOG_FILE.open("a") as f:
        f.write(f"{datetime.datetime.now().isoformat(timespec='seconds')} [{level}] {message}\n")
    if notify:
        urgency = {"error": "critical", "warn": "normal"}.get(level, "low")
        subprocess.run(
            ["notify-send", "-a", "Sofle", "-u", urgency, "-h", "string:x-dunst-stack-tag:sofle-flash", "Sofle", message],
            check=False,
        )


def uf2_version(path):
    """Versión embebida en el .uf2 (el número de serie USB, en UTF-16), o None si no la trae."""
    data = path.read_bytes()
    image = bytearray()
    for offset in range(0, len(data), 512):
        block = data[offset : offset + 512]
        size = struct.unpack("<I", block[16:20])[0]
        image += block[32 : 32 + size]
    match = re.search(re.escape(SERIAL_PREFIX.encode("utf-16-le")) + rb"((?:[\x20-\x7e]\x00)+)", bytes(image))
    return match.group(1).decode("utf-16-le") if match else None


def pending_firmware():
    """El .uf2 pendiente de cada mitad; si hubiera varios, el más reciente."""
    pending = {}
    for side, prefix in PENDING_PREFIXES.items():
        files = sorted(PENDING_DIR.glob(f"{prefix}*.uf2"), key=lambda f: f.stat().st_mtime)
        if files:
            pending[side] = files[-1]
    return pending


def bootloader_device():
    """Primera unidad RPI-RP2 conectada: (dispositivo, punto de montaje) o None."""
    out = subprocess.run(["lsblk", "-J", "-p", "-o", "NAME,LABEL,MOUNTPOINT"], capture_output=True, text=True).stdout
    stack = json.loads(out).get("blockdevices", [])
    while stack:
        dev = stack.pop()
        if dev.get("label") == "RPI-RP2":
            return dev["name"], dev.get("mountpoint")
        stack.extend(dev.get("children", []))
    return None


def device_present(name):
    out = subprocess.run(["lsblk", "-rpno", "NAME"], capture_output=True, text=True).stdout.split()
    return name in out


def mount(device, mount_point):
    if mount_point:
        return mount_point
    subprocess.run(["udisksctl", "mount", "-b", device], capture_output=True, check=False)
    return bootloader_device_mount(device)


def bootloader_device_mount(device):
    out = subprocess.run(["lsblk", "-rno", "MOUNTPOINT", device], capture_output=True, text=True).stdout.strip()
    return out or None


def side_of(mount_point):
    for side, pattern in MOUNT_PATTERNS.items():
        if mount_point and pattern in mount_point:
            return side
    return None


def keyboard_serial():
    """Número de serie USB del Sofle conectado (la versión de la mitad USB), o None."""
    for dev in Path("/sys/bus/usb/devices").iterdir():
        try:
            if (dev / "idVendor").read_text().strip() == KEYBOARD_VID and (dev / "idProduct").read_text().strip() == KEYBOARD_PID:
                return (dev / "serial").read_text().strip() if (dev / "serial").exists() else ""
        except OSError:
            continue
    return None


def wait_for(condition, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = condition()
        if result:
            return result
        time.sleep(0.5)
    return None


def backup_path(side):
    return Path(f"/tmp/sofle-flash-backup-{side}.json")


def refresh_backup():
    """Respaldo de la mitad conectada por USB, antes de que entre en modo de carga."""
    try:
        with sofle_hid.Keyboard() as kb:
            side = USB_SIDE_NAMES.get(kb.usb_side())
            backup = sofle_hid.take_backup(kb)
    except (sofle_hid.KeyboardError, OSError):
        return
    if side is None:
        return
    target = backup_path(side)
    is_new = not target.exists()
    target.write_text(json.dumps(backup, indent=1))
    if is_new:
        log("info", f"Respaldo previo al flasheo de la mitad {SIDE_NAMES[side]} guardado en {target}")


def restore_after_flash(side):
    """Restaura en la mitad recién flasheada el respaldo tomado antes, y lo borra."""
    source = backup_path(side)
    if not source.exists():
        return
    time.sleep(SETTLE_SECONDS)
    try:
        with sofle_hid.Keyboard() as kb:
            restored = sofle_hid.restore_backup(kb, json.loads(source.read_text()), log=lambda message: log("warn", message))
    except (sofle_hid.KeyboardError, OSError) as error:
        log("error", f"No se pudo restaurar el respaldo de la mitad {SIDE_NAMES[side]}: {error}. Queda en {source}", notify=True)
        return
    source.unlink()
    log("ok", f"Mitad {SIDE_NAMES[side]}: restaurado {restored}", notify=True)


def flash(side, firmware, device, mount_point):
    expected = uf2_version(firmware)
    log("info", f"Mitad {SIDE_NAMES[side]}: copiando {firmware.name} (versión {expected or 'sin versión'})", notify=True)
    shutil.copy(firmware, Path(mount_point) / firmware.name)
    os.sync()

    if not wait_for(lambda: not device_present(device), REBOOT_TIMEOUT):
        log("error", f"La mitad {SIDE_NAMES[side]} no se reinició tras la copia; el pendiente se conserva", notify=True)
        return False

    serial = wait_for(keyboard_serial, ENUMERATE_TIMEOUT)
    if serial is None:
        log("error", f"La mitad {SIDE_NAMES[side]} no volvió a aparecer como teclado; el pendiente se conserva", notify=True)
        return False
    running = serial[len(SERIAL_PREFIX) :] if serial.startswith(SERIAL_PREFIX) else serial
    if expected and running != expected:
        log("error", f"La mitad {SIDE_NAMES[side]} corre '{running or '?'}' y se esperaba '{expected}'; el pendiente se conserva", notify=True)
        return False

    FLASHED_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    shutil.move(firmware, FLASHED_DIR / f"{stamp}_{firmware.name}")
    log("ok", f"Mitad {SIDE_NAMES[side]} verificada: corre {running or expected}", notify=True)
    restore_after_flash(side)
    return True


def main():
    PENDING_DIR.mkdir(parents=True, exist_ok=True)
    log("info", f"Vigilando {PENDING_DIR} (log en {LOG_FILE})")
    last_pending = None
    handled_device = None
    next_backup = 0.0

    while True:
        pending = pending_firmware()
        if set(pending) != last_pending:
            if pending:
                names = ", ".join(SIDE_NAMES[side] for side in pending)
                log("info", f"Pendiente para: {names}. Pon esa mitad en modo de carga (doble toque al reset).", notify=True)
            elif last_pending:
                log("ok", "Las dos mitades están al día. Devuelve el USB a la derecha.", notify=True)
            else:
                log("wait", "Nada pendiente; esperando firmware en la carpeta.")
            last_pending = set(pending)

        # Con firmware pendiente y el teclado funcionando, mantiene fresco el respaldo previo
        if pending and keyboard_serial() is not None and time.monotonic() >= next_backup:
            refresh_backup()
            next_backup = time.monotonic() + BACKUP_REFRESH_SECONDS

        found = bootloader_device()
        if found is None:
            handled_device = None
        elif found[0] != handled_device:
            device, mount_point = found
            handled_device = device
            mount_point = mount(device, mount_point)
            side = side_of(mount_point)
            if side is None:
                log("warn", f"Entró una mitad que no reconozco ({mount_point}); no toco nada", notify=True)
            elif side not in pending:
                other = [SIDE_NAMES[s] for s in pending]
                hint = f" Pasa el USB a la {other[0]}." if other else ""
                log("warn", f"Entró la mitad {SIDE_NAMES[side]} pero no tiene firmware pendiente; no toco nada.{hint}", notify=True)
                subprocess.run(["udisksctl", "unmount", "-b", device], capture_output=True, check=False)
            else:
                flash(side, pending[side], device, mount_point)

        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("info", "Vigilancia detenida")
        sys.exit(0)
