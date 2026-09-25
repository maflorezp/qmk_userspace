#!/usr/bin/env python3
"""Habla con el Sofle por Raw HID (Linux, /dev/hidraw*), sin dependencias externas.

Ejemplos:
  sofle_hid.py ping
  sofle_hid.py version                # versión de cada mitad y si coinciden
  sofle_hid.py time                   # fija en el teclado la fecha y hora del PC
  sofle_hid.py layer
  sofle_hid.py oled "Hola desde el PC"
  sofle_hid.py colors                 # lista los colores por capa
  sofle_hid.py color lower 191 255    # cambia tono y saturación de LOWER (0-255) y guarda
  sofle_hid.py enable caps off        # desactiva el color de Caps Lock y guarda
  sofle_hid.py limit                  # muestra el brillo máximo (%)
  sofle_hid.py limit 60               # fija el brillo máximo en 60 % y guarda
  sofle_hid.py backup                 # respaldo completo en ~/Documents/QMK/sofle-backup-<fecha>.json
  sofle_hid.py backup /ruta/archivo.json
  sofle_hid.py restore /ruta/archivo.json
  sofle_hid.py boot                   # pone en modo de carga la mitad conectada por USB

Varios programas pueden usar el teclado a la vez (esta herramienta, el watcher y el agente):
un candado en /tmp/sofle-hid.lock hace que se turnen.
"""
import argparse
import datetime
import fcntl
import glob
import json
import os
import select
import sys
import time
from pathlib import Path

RAW_USAGE_PAGE = 0xFF60
PACKET_SIZE = 32
LOCK_FILE = "/tmp/sofle-hid.lock"
LOCK_TIMEOUT_SECONDS = 30
RESPONSE_TIMEOUT_SECONDS = 1

MATRIX_ROWS = 10
MATRIX_COLS = 6
NUM_ENCODERS = 2
BUFFER_CHUNK = 28  # datos útiles por paquete en las lecturas y escrituras de buffers de VIA

DEFAULT_BACKUP_DIR = Path.home() / "Documents" / "QMK"

# Protocolo propio (hid_protocol.c)
CMD_PING = 0x80
CMD_GET_LAYER = 0x81
CMD_VERSION = 0x82
CMD_OTHER_HALF = 0x83
CMD_USB_SIDE = 0x84
CMD_SET_TIME = 0x85
CMD_SCHEMA = 0x86
CMD_BOOTLOADER = 0x87
CMD_OLED_TEXT = 0x90

# Protocolo de VIA (quantum/via.h)
VIA_GET_LAYER_COUNT = 0x11
VIA_KEYMAP_GET_BUFFER = 0x12
VIA_KEYMAP_SET_BUFFER = 0x13
VIA_GET_ENCODER = 0x14
VIA_SET_ENCODER = 0x15
VIA_MACRO_GET_COUNT = 0x0C
VIA_MACRO_GET_BUFFER_SIZE = 0x0D
VIA_MACRO_GET_BUFFER = 0x0E
VIA_MACRO_SET_BUFFER = 0x0F
VIA_MACRO_RESET = 0x10
VIA_CUSTOM_SET = 0x07
VIA_CUSTOM_GET = 0x08
VIA_CUSTOM_SAVE = 0x09
VIA_UNHANDLED = 0xFF

# Canales de valores de VIA
VIA_CUSTOM_CHANNEL = 0  # menú propio (via_menu.c)
VIA_RGBLIGHT_CHANNEL = 2  # pestaña Lighting
RGBLIGHT_VALUES = {"brightness": 1, "effect": 2, "speed": 3, "color": 4}

# Menú propio (via_menu.c): 1..16 colores por capa (impar = activo, par = tono y saturación),
# 17..24 valores de 1 byte
CUSTOM_VALUE_IDS = range(1, 25)
VALUE_ID_BRIGHTNESS_LIMIT = 17

ITEMS = ["manual", "lower", "raise", "numeric", "adjust", "rgb", "caps", "macro"]
LAYER_NAMES = ["QWERTY", "LOWER", "RAISE", "NUMERIC", "ADJUST", "RGB"]


class KeyboardError(Exception):
    pass


def find_device():
    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(f"{node}/device/uevent") as f:
                if "Sofle" not in f.read():
                    continue
            with open(f"{node}/device/report_descriptor", "rb") as f:
                descriptor = f.read()
        except OSError:
            continue
        if descriptor[:3] == bytes([0x06, RAW_USAGE_PAGE & 0xFF, RAW_USAGE_PAGE >> 8]):
            return f"/dev/{os.path.basename(node)}"
    return None


class Keyboard:
    """Conexión Raw HID con el teclado; mientras está abierta, ningún otro programa la usa."""

    def __init__(self):
        self.lock = open(LOCK_FILE, "w")
        deadline = time.monotonic() + LOCK_TIMEOUT_SECONDS
        while True:
            try:
                fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() > deadline:
                    raise KeyboardError("Otro programa está usando el teclado (candado ocupado)")
                time.sleep(0.2)
        device = find_device()
        if device is None:
            self.close()
            raise KeyboardError("No encontré la interfaz Raw HID del Sofle (¿está conectado?)")
        self.fd = os.open(device, os.O_RDWR)

    def close(self):
        if getattr(self, "fd", None) is not None:
            os.close(self.fd)
            self.fd = None
        self.lock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def request(self, *payload):
        """Envía un paquete y devuelve la respuesta a ese mismo comando."""
        command = payload[0]
        # Descarta respuestas viejas que hayan quedado en la cola
        while select.select([self.fd], [], [], 0)[0]:
            os.read(self.fd, PACKET_SIZE)
        os.write(self.fd, b"\0" + bytes(payload).ljust(PACKET_SIZE, b"\0"))
        deadline = time.monotonic() + RESPONSE_TIMEOUT_SECONDS
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.fd], [], [], remaining)[0]:
                raise KeyboardError(f"El teclado no respondió al comando {command:#04x}")
            response = os.read(self.fd, PACKET_SIZE)
            # Otras aplicaciones (por ejemplo usevia.app) pueden recibir sus propias respuestas
            if response[0] in (command, VIA_UNHANDLED):
                return response

    def get_item(self, index):
        enabled = self.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, index * 2 + 1)[3]
        color = self.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, index * 2 + 2)
        return enabled, color[3], color[4]

    def firmware_version(self):
        return self.request(CMD_VERSION)[1:].split(b"\0")[0].decode()

    def usb_side(self):
        return "izquierda" if self.request(CMD_USB_SIDE)[1] else "derecha"

    def schema(self):
        response = self.request(CMD_SCHEMA)
        return response[1] if response[0] == CMD_SCHEMA else None

    def set_time(self, moment):
        response = self.request(CMD_SET_TIME, moment.year & 0xFF, moment.year >> 8, moment.month, moment.day, moment.hour, moment.minute, moment.second)
        return response[0] == CMD_SET_TIME and response[1] == 1

    # --- Buffers de VIA (keymap y macros) ---
    def _read_buffer(self, command, size):
        data = bytearray()
        for offset in range(0, size, BUFFER_CHUNK):
            chunk = min(BUFFER_CHUNK, size - offset)
            response = self.request(command, offset >> 8, offset & 0xFF, chunk)
            data += response[4 : 4 + chunk]
        return bytes(data)

    def _write_buffer(self, command, data):
        for offset in range(0, len(data), BUFFER_CHUNK):
            chunk = data[offset : offset + BUFFER_CHUNK]
            self.request(command, offset >> 8, offset & 0xFF, len(chunk), *chunk)


def take_backup(kb):
    """Todo lo que vive en la memoria del teclado, como diccionario serializable en JSON."""
    layers = kb.request(VIA_GET_LAYER_COUNT)[1]
    keymap = kb._read_buffer(VIA_KEYMAP_GET_BUFFER, layers * MATRIX_ROWS * MATRIX_COLS * 2)

    encoders = []
    for layer in range(layers):
        for encoder in range(NUM_ENCODERS):
            for clockwise in (0, 1):
                response = kb.request(VIA_GET_ENCODER, layer, encoder, clockwise)
                encoders.append([layer, encoder, clockwise, (response[4] << 8) | response[5]])

    macro_count = kb.request(VIA_MACRO_GET_COUNT)[1]
    size_response = kb.request(VIA_MACRO_GET_BUFFER_SIZE)
    macro_size = (size_response[1] << 8) | size_response[2]
    macros = kb._read_buffer(VIA_MACRO_GET_BUFFER, macro_size).rstrip(b"\0")

    rgblight = {}
    for name, value_id in RGBLIGHT_VALUES.items():
        response = kb.request(VIA_CUSTOM_GET, VIA_RGBLIGHT_CHANNEL, value_id)
        rgblight[name] = list(response[3:5]) if name == "color" else response[3]

    schema = kb.schema()
    custom = {}
    if schema is not None:
        for value_id in CUSTOM_VALUE_IDS:
            response = kb.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, value_id)
            is_color = value_id <= len(ITEMS) * 2 and value_id % 2 == 0
            custom[str(value_id)] = list(response[3:5]) if is_color else [response[3]]

    return {
        "format": 1,
        "created": datetime.datetime.now().isoformat(timespec="seconds"),
        "firmware": kb.firmware_version(),
        "usb_side": kb.usb_side(),
        "layers": layers,
        "keymap": keymap.hex(),
        "encoders": encoders,
        "macros": {"count": macro_count, "data": macros.hex()},
        "rgblight": rgblight,
        "schema": schema,
        "custom": custom,
    }


def backup_content(backup):
    """Lo que importa para comparar dos respaldos (sin fecha ni versión del firmware)."""
    return {key: value for key, value in backup.items() if key not in ("created", "firmware")}


def restore_backup(kb, backup, log=print):
    """Restaura un respaldo; la configuración propia solo si la estructura del firmware coincide."""
    if backup.get("format") != 1:
        raise KeyboardError("Formato de respaldo desconocido")
    layers = kb.request(VIA_GET_LAYER_COUNT)[1]
    if layers != backup["layers"]:
        raise KeyboardError(f"El teclado tiene {layers} capas y el respaldo {backup['layers']}: no se restaura")

    kb._write_buffer(VIA_KEYMAP_SET_BUFFER, bytes.fromhex(backup["keymap"]))
    for layer, encoder, clockwise, keycode in backup["encoders"]:
        kb.request(VIA_SET_ENCODER, layer, encoder, clockwise, keycode >> 8, keycode & 0xFF)

    kb.request(VIA_MACRO_RESET)
    macros = bytes.fromhex(backup["macros"]["data"])
    if macros:
        kb._write_buffer(VIA_MACRO_SET_BUFFER, macros + b"\0")

    for name, value_id in RGBLIGHT_VALUES.items():
        value = backup["rgblight"][name]
        kb.request(VIA_CUSTOM_SET, VIA_RGBLIGHT_CHANNEL, value_id, *(value if isinstance(value, list) else [value]))
    kb.request(VIA_CUSTOM_SAVE, VIA_RGBLIGHT_CHANNEL, 0)
    restored = "capas, encoders, macros y tira (Lighting)"

    schema = kb.schema()
    if backup["schema"] is not None and backup["schema"] == schema:
        for value_id, value in backup["custom"].items():
            kb.request(VIA_CUSTOM_SET, VIA_CUSTOM_CHANNEL, int(value_id), *value)
        kb.request(VIA_CUSTOM_SAVE, VIA_CUSTOM_CHANNEL, 0)
        restored += " y configuración propia"
    else:
        log(f"La estructura de la configuración propia cambió ({backup['schema']} -> {schema}): esa parte no se restauró")
    return restored


def item_index(name):
    if name not in ITEMS:
        sys.exit(f"Elemento desconocido '{name}'. Opciones: {', '.join(ITEMS)}")
    return ITEMS.index(name)


def main():
    parser = argparse.ArgumentParser(description="Comunicación con el Sofle por Raw HID")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("ping", help="comprueba que el teclado responde")
    sub.add_parser("version", help="muestra la versión de cada mitad y si coinciden")
    sub.add_parser("time", help="fija en el teclado la fecha y hora del PC")
    sub.add_parser("layer", help="muestra la capa activa")
    oled = sub.add_parser("oled", help="muestra un texto en la OLED por 5 segundos")
    oled.add_argument("text")
    sub.add_parser("colors", help="lista los colores por capa")
    color = sub.add_parser("color", help="cambia el color de un elemento y lo guarda")
    color.add_argument("item")
    color.add_argument("hue", type=int)
    color.add_argument("sat", type=int)
    enable = sub.add_parser("enable", help="activa o desactiva el color de un elemento y lo guarda")
    enable.add_argument("item")
    enable.add_argument("state", choices=["on", "off"])
    limit = sub.add_parser("limit", help="muestra o fija el brillo máximo de la tira (10-100 %%)")
    limit.add_argument("percent", type=int, nargs="?")
    backup = sub.add_parser("backup", help="guarda un respaldo completo de lo que hay en el teclado")
    backup.add_argument("path", nargs="?", help="archivo de destino (por defecto en ~/Documents/QMK/)")
    sub.add_parser("boot", help="pone en modo de carga la mitad conectada por USB")
    restore = sub.add_parser("restore", help="restaura un respaldo en el teclado")
    restore.add_argument("path")
    args = parser.parse_args()

    try:
        with Keyboard() as kb:
            run(kb, args)
    except KeyboardError as error:
        sys.exit(str(error))


def run(kb, args):
    if args.command == "ping":
        r = kb.request(CMD_PING)
        print(f"OK: {chr(r[1])}{chr(r[2])} protocolo v{r[3]}" if r[0] == CMD_PING else f"Respuesta inesperada: {r.hex()}")
    elif args.command == "version":
        other = kb.request(CMD_OTHER_HALF)
        state = {0: "sin respuesta", 1: "coinciden", 2: "NO COINCIDEN: flashear las dos"}.get(other[1], "?")
        print(f"Mitad USB ({kb.usb_side()}): {kb.firmware_version()}")
        other_version = other[2:].split(b"\0")[0].decode()
        print(f"Otra mitad: {other_version or '-'}")
        print(f"Estado:     {state}")
    elif args.command == "time":
        now = datetime.datetime.now()
        print(f"Hora fijada: {now:%d/%m/%Y %H:%M:%S}" if kb.set_time(now) else "El teclado no aceptó la hora")
    elif args.command == "layer":
        layer = kb.request(CMD_GET_LAYER)[1]
        print(LAYER_NAMES[layer] if layer < len(LAYER_NAMES) else layer)
    elif args.command == "oled":
        kb.request(CMD_OLED_TEXT, *args.text.encode()[: PACKET_SIZE - 2])
    elif args.command == "colors":
        for index, name in enumerate(ITEMS):
            enabled, hue, sat = kb.get_item(index)
            print(f"{name:8} {'activo  ' if enabled else 'inactivo'} tono={hue:3} saturación={sat:3}")
    elif args.command == "color":
        index = item_index(args.item)
        kb.request(VIA_CUSTOM_SET, VIA_CUSTOM_CHANNEL, index * 2 + 2, args.hue & 0xFF, args.sat & 0xFF)
        kb.request(VIA_CUSTOM_SAVE, VIA_CUSTOM_CHANNEL, 0)
    elif args.command == "enable":
        index = item_index(args.item)
        kb.request(VIA_CUSTOM_SET, VIA_CUSTOM_CHANNEL, index * 2 + 1, 1 if args.state == "on" else 0)
        kb.request(VIA_CUSTOM_SAVE, VIA_CUSTOM_CHANNEL, 0)
    elif args.command == "limit":
        if args.percent is not None:
            kb.request(VIA_CUSTOM_SET, VIA_CUSTOM_CHANNEL, VALUE_ID_BRIGHTNESS_LIMIT, args.percent & 0xFF)
            kb.request(VIA_CUSTOM_SAVE, VIA_CUSTOM_CHANNEL, VALUE_ID_BRIGHTNESS_LIMIT)
        print(f"Brillo máximo: {kb.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, VALUE_ID_BRIGHTNESS_LIMIT)[3]} %")
    elif args.command == "backup":
        path = Path(args.path) if args.path else DEFAULT_BACKUP_DIR / f"sofle-backup-{datetime.datetime.now():%Y%m%d-%H%M%S}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(take_backup(kb), indent=1))
        print(f"Respaldo guardado en {path}")
    elif args.command == "boot":
        r = kb.request(CMD_BOOTLOADER, ord("B"), ord("L"))
        print("Entrando en modo de carga" if r[0] == CMD_BOOTLOADER and r[1] == 1 else "Este firmware no admite el modo de carga por HID")
    elif args.command == "restore":
        restored = restore_backup(kb, json.loads(Path(args.path).read_text()))
        print(f"Restaurado: {restored}")


if __name__ == "__main__":
    main()
