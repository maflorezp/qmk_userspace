#!/usr/bin/env python3
"""Flashea el Sofle desde firmware/pending/, mitad por mitad, con avisos en pantalla (eww).

Uso: tools/flash_watcher.py [--countdown 10] [--once] [--no-popup] [--home-side right]
  --countdown N  segundos de cuenta regresiva antes de poner en modo de carga, por HID, la mitad
                 conectada que tenga firmware pendiente (0 = no automático: doble toque al reset)
  --once         termina cuando no queda nada pendiente (así lo usa el servicio de systemd, que
                 lo arranca una unidad .path cuando aparece un .uf2 en la carpeta)
  --no-popup     sin ventana de eww (solo terminal, notificaciones y log)
  --home-side    mitad donde va el USB normalmente (por defecto right): al terminar, si el USB quedó
                 en la otra, la ventana pide devolverlo y espera a verlo ahí

- firmware/pending/sofle_L-<versión>.uf2 va a la mitad izquierda y sofle_R-<versión>.uf2 a la derecha.
- Cada mitad se reconoce por el punto donde el sistema monta su unidad RPI-RP2
  (PicoL / PicoR; se cambia con LEFT_MOUNT_PATTERN / RIGHT_MOUNT_PATTERN).
- Antes de flashear guarda un respaldo de la mitad (capas, macros, luces y configuración) en /tmp;
  tras copiar el .uf2 espera a que la mitad arranque, compara la versión que reporta por USB con
  la del .uf2 y, si coinciden, restaura el respaldo y mueve el .uf2 a firmware/flashed/.
- La ventana de eww (tools/eww) queda visible hasta que las dos mitades están al día. Sus botones
  escriben en /tmp/sofle-flash-control: "now" (flashear ya) o "postpone" (posponer 5 minutos).
- Log en /tmp/sofle-flash.log.
"""
import argparse
import datetime
import json
import math
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
EWW_CONFIG = REPO_DIR / "tools" / "eww"
EWW_WINDOW = "sofle-flash"
LOG_FILE = Path("/tmp/sofle-flash.log")
CONTROL_FILE = Path("/tmp/sofle-flash-control")

PENDING_PREFIXES = {"left": "sofle_L", "right": "sofle_R"}
SIDE_NAMES = {"left": "izquierda", "right": "derecha", None: "desconocida"}
MOUNT_PATTERNS = {
    "left": os.environ.get("LEFT_MOUNT_PATTERN", "PicoL"),
    "right": os.environ.get("RIGHT_MOUNT_PATTERN", "PicoR"),
}

KEYBOARD_VID, KEYBOARD_PID = "fc32", "0287"
SERIAL_PREFIX = "maflorezp-"
BOOTLOADER_MIN_PROTOCOL = 7  # desde esta versión del protocolo HID el teclado entra en modo de carga por HID
POLL_SECONDS = 0.5
BACKUP_REFRESH_SECONDS = 30
POSTPONE_SECONDS = 300
SETTLE_SECONDS = 3  # espera tras el reinicio, a que el teclado termine de arrancar
DONE_DISPLAY_SECONDS = 5
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


class Popup:
    """Ventana de eww al centro de la pantalla; solo se actualiza cuando cambia lo que muestra."""

    def __init__(self, enabled):
        self.enabled = enabled and shutil.which("eww") is not None
        self.is_open = False
        self.shown = None

    def _eww(self, *args):
        return subprocess.run(["eww", "-c", str(EWW_CONFIG), *args], capture_output=True, text=True, check=False)

    def show(self, title, message, level="info", countdown="", detail="", buttons=False):
        state = (title, message, level, countdown, detail, buttons)
        if not self.enabled or state == self.shown:
            return
        if self._eww("ping").returncode != 0:
            subprocess.Popen(["eww", "-c", str(EWW_CONFIG), "daemon"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(1)
        self._eww(
            "update",
            f"sofle_title={title}",
            f"sofle_message={message}",
            f"sofle_level={level}",
            f"sofle_countdown={countdown}",
            f"sofle_detail={detail}",
            f"sofle_buttons={'true' if buttons else 'false'}",
        )
        if not self.is_open:
            self._eww("open", EWW_WINDOW)
            self.is_open = True
        self.shown = state

    def close(self):
        if self.enabled and self.is_open:
            self._eww("close", EWW_WINDOW)
        self.is_open = False
        self.shown = None


def read_control():
    """Orden de los botones de la ventana ("now" o "postpone"), que se consume al leerla."""
    try:
        command = CONTROL_FILE.read_text().strip()
        CONTROL_FILE.unlink()
        return command
    except OSError:
        return None


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


def keyboard_info():
    """Mitad conectada por USB y si acepta entrar en modo de carga por HID, o None si no responde."""
    try:
        with sofle_hid.Keyboard() as kb:
            side = USB_SIDE_NAMES.get(kb.usb_side())
            ping = kb.request(sofle_hid.CMD_PING)
            protocol = ping[3] if ping[0] == sofle_hid.CMD_PING else 0
    except (sofle_hid.KeyboardError, OSError):
        return None
    return side, protocol >= BOOTLOADER_MIN_PROTOCOL


def enter_bootloader():
    try:
        with sofle_hid.Keyboard() as kb:
            response = kb.request(sofle_hid.CMD_BOOTLOADER, ord("B"), ord("L"))
            return response[0] == sofle_hid.CMD_BOOTLOADER and response[1] == 1
    except (sofle_hid.KeyboardError, OSError):
        return False


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


def refresh_backup(side):
    """Respaldo de la mitad conectada por USB, antes de que entre en modo de carga."""
    try:
        with sofle_hid.Keyboard() as kb:
            backup = sofle_hid.take_backup(kb)
    except (sofle_hid.KeyboardError, OSError):
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
    log("ok", f"Mitad {SIDE_NAMES[side]}: restaurado {restored}")


def flash(side, firmware, device, mount_point, popup):
    expected = uf2_version(firmware)
    popup.show("Sofle", f"Flasheando la mitad {SIDE_NAMES[side]}…", detail=f"versión {expected or 'sin versión'}")
    log("info", f"Mitad {SIDE_NAMES[side]}: copiando {firmware.name} (versión {expected or 'sin versión'})")
    shutil.copy(firmware, Path(mount_point) / firmware.name)
    os.sync()

    if not wait_for(lambda: not device_present(device), REBOOT_TIMEOUT):
        log("error", f"La mitad {SIDE_NAMES[side]} no se reinició tras la copia; el pendiente se conserva", notify=True)
        return False

    popup.show("Sofle", f"Verificando la mitad {SIDE_NAMES[side]}…", detail=f"esperando que arranque con {expected}")
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
    log("ok", f"Mitad {SIDE_NAMES[side]} verificada: corre {running or expected}")
    popup.show("Sofle", f"Mitad {SIDE_NAMES[side]} lista", level="ok", detail=f"restaurando su configuración ({running})")
    restore_after_flash(side)
    return True


class Watcher:
    def __init__(self, args):
        self.args = args
        self.popup = Popup(not args.no_popup)
        self.handled_device = None
        self.countdown_until = None
        self.postponed_until = 0.0
        self.next_backup = 0.0
        self.last_version = None
        self.had_pending = False

    def run(self):
        PENDING_DIR.mkdir(parents=True, exist_ok=True)
        CONTROL_FILE.unlink(missing_ok=True)
        log("info", f"Vigilando {PENDING_DIR} (cuenta regresiva {self.args.countdown} s, log en {LOG_FILE})")
        while True:
            pending = pending_firmware()
            if not pending:
                if self.had_pending:
                    log("ok", f"Las dos mitades están al día ({self.last_version})", notify=True)
                    self.wait_home_side()
                    self.popup.show("Sofle", "Las dos mitades están al día", level="ok", detail=f"versión {self.last_version}")
                    time.sleep(DONE_DISPLAY_SECONDS)
                    self.popup.close()
                    self.had_pending = False
                if self.args.once:
                    log("info", "Nada pendiente: termino")
                    return
                time.sleep(POLL_SECONDS)
                continue

            if not self.had_pending:
                names = " y ".join(SIDE_NAMES[side] for side in pending)
                log("info", f"Firmware pendiente para la mitad {names}")
            self.had_pending = True
            self.step(pending)
            time.sleep(POLL_SECONDS)

    def wait_home_side(self):
        """Si el USB quedó en la otra mitad, pide devolverlo y espera a verlo en la de siempre."""
        home = self.args.home_side
        info = keyboard_info()
        if info is not None and info[0] == home:
            return
        log("info", f"Esperando que el USB vuelva a la mitad {SIDE_NAMES[home]}")
        while True:
            info = keyboard_info() if keyboard_serial() is not None else None
            if info is not None and info[0] == home:
                log("ok", f"USB de vuelta en la mitad {SIDE_NAMES[home]}")
                return
            self.popup.show("Sofle · listo", f"Devuelve el USB a la mitad {SIDE_NAMES[home]}", level="warn", detail=f"las dos mitades ya tienen la versión {self.last_version}")
            time.sleep(POLL_SECONDS)

    def step(self, pending):
        control = read_control()
        found = bootloader_device()
        if found is None:
            self.handled_device = None
        elif found[0] != self.handled_device:
            self.countdown_until = None
            self.handle_bootloader(found, pending)
            return
        else:
            return  # unidad ya atendida: espera a que desaparezca

        if keyboard_serial() is None:
            names = " o ".join(SIDE_NAMES[side] for side in pending)
            self.popup.show("Sofle · firmware pendiente", f"Conecta el USB a la mitad {names}", level="warn")
            return

        info = keyboard_info()
        if info is None:
            return  # el teclado está arrancando o lo usa otro programa: reintenta en el siguiente ciclo
        usb_side, can_boot = info

        if usb_side not in pending:
            other = next(iter(pending))
            self.countdown_until = None
            self.popup.show("Sofle · firmware pendiente", f"Pasa el USB a la mitad {SIDE_NAMES[other]}", level="warn", detail=f"la mitad {SIDE_NAMES[usb_side]} ya está al día")
            return

        now = time.monotonic()
        if now >= self.next_backup:
            refresh_backup(usb_side)
            self.next_backup = now + BACKUP_REFRESH_SECONDS

        firmware = pending[usb_side]
        self.last_version = uf2_version(firmware) or firmware.name
        if control == "postpone":
            self.postponed_until = now + POSTPONE_SECONDS
            self.countdown_until = None
            log("info", f"Flasheo de la mitad {SIDE_NAMES[usb_side]} pospuesto {POSTPONE_SECONDS // 60} minutos")
        if control == "now":
            self.postponed_until = 0.0

        if not can_boot or self.args.countdown == 0:
            self.popup.show("Sofle · firmware pendiente", f"Doble toque al reset de la mitad {SIDE_NAMES[usb_side]}", detail=f"versión {self.last_version}")
            return

        if now < self.postponed_until:
            resume = datetime.datetime.now() + datetime.timedelta(seconds=self.postponed_until - now)
            self.popup.show("Sofle · flasheo pospuesto", f"Mitad {SIDE_NAMES[usb_side]}: se retoma a las {resume:%H:%M}", level="warn", detail=f"versión {self.last_version}", buttons=True)
            return

        if self.countdown_until is None:
            self.countdown_until = now + self.args.countdown
        remaining = math.ceil(self.countdown_until - now)
        if control == "now" or remaining <= 0:
            self.countdown_until = None
            refresh_backup(usb_side)
            self.popup.show("Sofle", f"Entrando en modo de carga (mitad {SIDE_NAMES[usb_side]})…", detail=f"versión {self.last_version}")
            if enter_bootloader():
                log("info", f"Mitad {SIDE_NAMES[usb_side]} puesta en modo de carga por HID")
            else:
                log("warn", f"La mitad {SIDE_NAMES[usb_side]} no entró en modo de carga por HID: doble toque al reset", notify=True)
            return
        self.popup.show(
            "Sofle · firmware nuevo",
            f"Flasheando la mitad {SIDE_NAMES[usb_side]} en",
            countdown=str(remaining),
            detail=f"versión {self.last_version} · el teclado se desconecta unos segundos",
            buttons=True,
        )

    def handle_bootloader(self, found, pending):
        device, mount_point = found
        self.handled_device = device
        mount_point = mount(device, mount_point)
        side = side_of(mount_point)
        if side is None:
            log("warn", f"Entró una mitad que no reconozco ({mount_point}); no toco nada", notify=True)
        elif side not in pending:
            other = [SIDE_NAMES[s] for s in pending]
            log("warn", f"Entró la mitad {SIDE_NAMES[side]} pero no tiene firmware pendiente; no toco nada", notify=True)
            self.popup.show("Sofle", f"La mitad {SIDE_NAMES[side]} no tiene firmware pendiente", level="warn", detail=f"desconéctala y pasa el USB a la {other[0]}" if other else "")
            subprocess.run(["udisksctl", "unmount", "-b", device], capture_output=True, check=False)
        else:
            flash(side, pending[side], device, mount_point, self.popup)


def main():
    parser = argparse.ArgumentParser(description="Flashea el Sofle desde firmware/pending/ con avisos en pantalla")
    parser.add_argument("--countdown", type=int, default=10, help="segundos antes de poner la mitad en modo de carga por HID (0 = doble toque manual)")
    parser.add_argument("--once", action="store_true", help="terminar cuando no quede firmware pendiente")
    parser.add_argument("--no-popup", action="store_true", help="sin ventana de eww")
    parser.add_argument("--home-side", choices=["left", "right"], default="right", help="mitad donde va el USB normalmente (al terminar pide devolverlo ahí)")
    watcher = Watcher(parser.parse_args())
    try:
        watcher.run()
    finally:
        watcher.popup.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("info", "Vigilancia detenida")
        sys.exit(0)
