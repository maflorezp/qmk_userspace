#!/usr/bin/env python3
"""Diagnóstico de la pantalla a color del Sofle por Raw HID (lcd_probe.c), sin recompilar.

Ejemplos:
  lcd_probe.py pins 0                 # SCL, SDA, CS, DC y BL en bajo (para medir con el multímetro)
  lcd_probe.py pins sda bl            # solo SDA y BL en alto; el resto en bajo
  lcd_probe.py init                   # reinicia con lo del firmware (divisor 8, modo 3) y dibuja el patrón
  lcd_probe.py init --divisor 128 --mode 0 --rotation 0 --x 34 --y 0
  lcd_probe.py raw 0x21               # comando sin parámetros (0x21: invertir colores)
  lcd_probe.py raw 0x36 0x00          # comando con parámetros
  lcd_probe.py fill 0 255 255         # todo el panel en rojo (tono, saturación, brillo de 0 a 255)
  lcd_probe.py bitbang f800           # inicio y relleno en rojo por SPI por software (RGB565)
  lcd_probe.py bitbang 07e0 --no-invert
  lcd_probe.py demo on                # el teclado anima un cuadrado que rebota (redibujado en el firmware)
  lcd_probe.py demo on --step 6       # más rápido: píxeles que avanza en cada cuadro
  lcd_probe.py demo off               # detiene la animación y vuelve el patrón

Las órdenes se ejecutan en el teclado después de responder: la herramienta espera lo necesario.
"""
import argparse
import sys
import time

import sofle_hid

CMD_PINS = 0xA0
CMD_INIT = 0xA1
CMD_RAW = 0xA2
CMD_FILL = 0xA3
CMD_BITBANG = 0xA4
CMD_DEMO = 0xA5

PIN_BITS = {"scl": 0, "sda": 1, "cs": 2, "dc": 3, "bl": 4}
RAW_MAX_BYTES = 29
MIN_PROTOCOL = 8

# Lo que tarda cada orden en el teclado (el relleno por software escribe 110 KB a mano)
WAIT_SECONDS = {CMD_PINS: 0.1, CMD_INIT: 1.5, CMD_RAW: 0.2, CMD_FILL: 1.0, CMD_BITBANG: 6.0, CMD_DEMO: 0.2}


def number(text):
    return int(text, 0)


def send(kb, *payload):
    response = kb.request(*payload)
    if response[0] != payload[0] or response[1] != 1:
        raise sofle_hid.KeyboardError(
            "El teclado no reconoce la orden: ¿tiene el firmware con diagnóstico (protocolo v8)?"
        )
    time.sleep(WAIT_SECONDS[payload[0]])


def main():
    parser = argparse.ArgumentParser(description="Diagnóstico de la pantalla a color del Sofle")
    sub = parser.add_subparsers(dest="command", required=True)

    pins = sub.add_parser("pins", help="fija pines en alto; los no nombrados quedan en bajo")
    pins.add_argument("high", nargs="*", help=f"pines en alto: {', '.join(PIN_BITS)} (o 0 para todos en bajo)")

    init = sub.add_parser("init", help="reinicia el panel con otros parámetros y dibuja el patrón")
    init.add_argument("--divisor", type=int, default=8, help="125 MHz / divisor (por defecto 8)")
    init.add_argument("--mode", type=int, default=3, choices=range(4), help="modo SPI (por defecto 3)")
    init.add_argument("--rotation", type=int, default=0, choices=range(4), help="0, 1, 2 o 3 (x 90°)")
    init.add_argument("--x", type=int, default=34, help="desplazamiento de columnas (por defecto 34)")
    init.add_argument("--y", type=int, default=0, help="desplazamiento de filas (por defecto 0)")

    raw = sub.add_parser("raw", help="manda un comando del ST7789 y sus parámetros tal cual")
    raw.add_argument("opcode", type=number)
    raw.add_argument("params", type=number, nargs="*")

    fill = sub.add_parser("fill", help="pinta todo el panel de un color (tono, saturación, brillo)")
    fill.add_argument("hue", type=int)
    fill.add_argument("sat", type=int)
    fill.add_argument("val", type=int)

    bitbang = sub.add_parser("bitbang", help="inicio y relleno por SPI por software, sin el periférico")
    bitbang.add_argument("color", type=lambda text: int(text, 16), help="color RGB565 en hexadecimal (f800 rojo)")
    bitbang.add_argument("--no-invert", action="store_true", help="no invertir colores (por defecto se invierten)")

    demo = sub.add_parser("demo", help="enciende o apaga la animación del firmware (cuadrado que rebota)")
    demo.add_argument("state", choices=["on", "off"])
    demo.add_argument("--step", type=int, default=4, help="píxeles que avanza en cada cuadro (por defecto 4)")

    args = parser.parse_args()
    try:
        with sofle_hid.Keyboard() as kb:
            protocol = kb.request(sofle_hid.CMD_PING)[3]
            if protocol < MIN_PROTOCOL:
                sys.exit(f"El teclado tiene el protocolo v{protocol}; el diagnóstico necesita v{MIN_PROTOCOL}")
            run(kb, args)
    except sofle_hid.KeyboardError as error:
        sys.exit(str(error))


def run(kb, args):
    if args.command == "pins":
        names = [name.lower() for name in args.high if name != "0"]
        unknown = [name for name in names if name not in PIN_BITS]
        if unknown:
            sys.exit(f"Pines desconocidos: {', '.join(unknown)}. Opciones: {', '.join(PIN_BITS)}")
        levels = sum(1 << PIN_BITS[name] for name in names)
        send(kb, CMD_PINS, levels)
        print("En alto: " + (", ".join(names) or "ninguno") + ". El resto, en bajo.")
    elif args.command == "init":
        send(kb, CMD_INIT, args.divisor & 0xFF, args.divisor >> 8, args.mode, args.rotation, args.x, args.y)
        print(f"Reiniciado: divisor {args.divisor} ({125 / args.divisor:.2f} MHz), modo {args.mode}, "
              f"rotación {args.rotation}, desplazamiento ({args.x}, {args.y})")
    elif args.command == "raw":
        if len(args.params) > RAW_MAX_BYTES:
            sys.exit(f"Máximo {RAW_MAX_BYTES} parámetros")
        send(kb, CMD_RAW, args.opcode, len(args.params), *args.params)
        print(f"Enviado {args.opcode:#04x} " + " ".join(f"{p:#04x}" for p in args.params))
    elif args.command == "fill":
        send(kb, CMD_FILL, args.hue, args.sat, args.val)
        print(f"Relleno con tono {args.hue}, saturación {args.sat}, brillo {args.val}")
    elif args.command == "bitbang":
        send(kb, CMD_BITBANG, args.color >> 8, args.color & 0xFF, 0 if args.no_invert else 1)
        print(f"Relleno por software con {args.color:04x}" + ("" if args.no_invert else " (colores invertidos)"))
    elif args.command == "demo":
        send(kb, CMD_DEMO, 1 if args.state == "on" else 0, max(1, min(args.step, 255)))
        print("Animación encendida" if args.state == "on" else "Animación apagada: vuelve el patrón")


if __name__ == "__main__":
    main()
