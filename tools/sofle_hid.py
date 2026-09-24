#!/usr/bin/env python3
"""Habla con el Sofle por Raw HID (Linux, /dev/hidraw*), sin dependencias externas.

Ejemplos:
  sofle_hid.py ping
  sofle_hid.py version                # versión de cada mitad y si coinciden
  sofle_hid.py layer
  sofle_hid.py oled "Hola desde el PC"
  sofle_hid.py colors                 # lista los colores por capa
  sofle_hid.py color lower 191 255    # cambia tono y saturación de LOWER (0-255) y guarda
  sofle_hid.py enable caps off        # desactiva el color de Caps Lock y guarda
  sofle_hid.py limit                  # muestra el brillo máximo (%)
  sofle_hid.py limit 60               # fija el brillo máximo en 60 % y guarda
"""
import argparse
import glob
import os
import select
import sys

RAW_USAGE_PAGE = 0xFF60
PACKET_SIZE = 32

# Protocolo propio (hid_protocol.c)
CMD_PING = 0x80
CMD_GET_LAYER = 0x81
CMD_VERSION = 0x82
CMD_OTHER_HALF = 0x83
CMD_USB_SIDE = 0x84
CMD_OLED_TEXT = 0x90

# Valores propios de VIA (via_menu.c): canal 0, id impar = activo, id par = color
VIA_CUSTOM_SET = 0x07
VIA_CUSTOM_GET = 0x08
VIA_CUSTOM_SAVE = 0x09
VIA_CUSTOM_CHANNEL = 0

VALUE_ID_BRIGHTNESS_LIMIT = 17

ITEMS = ["manual", "lower", "raise", "numeric", "adjust", "rgb", "caps", "macro"]
LAYER_NAMES = ["QWERTY", "LOWER", "RAISE", "NUMERIC", "ADJUST", "RGB"]


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
    sys.exit("No encontré la interfaz Raw HID del Sofle (¿está conectado?)")


class Keyboard:
    def __init__(self):
        self.fd = os.open(find_device(), os.O_RDWR)

    def request(self, *payload):
        packet = bytes(payload).ljust(PACKET_SIZE, b"\0")
        os.write(self.fd, b"\0" + packet)
        ready, _, _ = select.select([self.fd], [], [], 1)
        if not ready:
            sys.exit("El teclado no respondió")
        return os.read(self.fd, PACKET_SIZE)

    def get_item(self, index):
        enabled = self.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, index * 2 + 1)[3]
        color = self.request(VIA_CUSTOM_GET, VIA_CUSTOM_CHANNEL, index * 2 + 2)
        return enabled, color[3], color[4]


def item_index(name):
    if name not in ITEMS:
        sys.exit(f"Elemento desconocido '{name}'. Opciones: {', '.join(ITEMS)}")
    return ITEMS.index(name)


def main():
    parser = argparse.ArgumentParser(description="Comunicación con el Sofle por Raw HID")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("ping", help="comprueba que el teclado responde")
    sub.add_parser("version", help="muestra la versión de cada mitad y si coinciden")
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
    args = parser.parse_args()

    kb = Keyboard()
    if args.command == "ping":
        r = kb.request(CMD_PING)
        print(f"OK: {chr(r[1])}{chr(r[2])} protocolo v{r[3]}" if r[0] == CMD_PING else f"Respuesta inesperada: {r.hex()}")
    elif args.command == "version":
        own = kb.request(CMD_VERSION)[1:].split(b"\0")[0].decode()
        other = kb.request(CMD_OTHER_HALF)
        state = {0: "sin respuesta", 1: "coinciden", 2: "NO COINCIDEN: flashear las dos"}.get(other[1], "?")
        side = "izquierda" if kb.request(CMD_USB_SIDE)[1] else "derecha"
        print(f"Mitad USB ({side}): {own}")
        other_version = other[2:].split(b"\0")[0].decode()
        print(f"Otra mitad: {other_version or '-'}")
        print(f"Estado:     {state}")
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


if __name__ == "__main__":
    main()
