#!/usr/bin/env python3
"""Agente del Sofle para correr como servicio de usuario (systemd).

- Envía la fecha y la hora al teclado cada --time-minutes minutos y apenas se conecta o
  reinicia (por ejemplo después de flashear), porque el teclado no tiene reloj con batería.
- Cada --backup-hours horas saca un respaldo completo a /tmp y lo compara con el último
  guardado en --backup-dir: si no cambió nada no escribe; si cambió, lo guarda con fecha y hora.
- Al conectarse el teclado, avisa (notify-send) si las dos mitades tienen firmware distinto.

Uso: sofle_agent.py [--time-minutes 15] [--backup-hours 6] [--backup-dir ~/Documents/QMK/backups]
Los mensajes salen por la salida estándar (en systemd quedan en journalctl --user -u sofle-agent).
"""
import argparse
import datetime
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sofle_hid  # noqa: E402

POLL_SECONDS = 5
KEYBOARD_VID, KEYBOARD_PID = "fc32", "0287"
SETTLE_SECONDS = 3  # espera tras conectarse, a que el teclado termine de arrancar
TEMP_BACKUP = Path("/tmp/sofle-agent-backup.json")


def log(message):
    print(f"{datetime.datetime.now():%Y-%m-%d %H:%M:%S} {message}", flush=True)


def keyboard_connection():
    """Identificador de la conexión USB actual del teclado (cambia al reconectar), o None."""
    for dev in Path("/sys/bus/usb/devices").iterdir():
        try:
            if (dev / "idVendor").read_text().strip() == KEYBOARD_VID and (dev / "idProduct").read_text().strip() == KEYBOARD_PID:
                return f"{dev.name}:{(dev / 'devnum').read_text().strip()}"
        except OSError:
            continue
    return None


def sync_time():
    with sofle_hid.Keyboard() as kb:
        now = datetime.datetime.now()
        if kb.set_time(now):
            log(f"Hora enviada: {now:%d/%m/%Y %H:%M:%S}")
        else:
            log("El teclado no aceptó la hora")


PENDING_DIR = Path(__file__).resolve().parent.parent / "firmware" / "pending"


def check_halves():
    """Avisa si las dos mitades corren firmware distinto (hay que flashear las dos).

    Mientras haya firmware en la cola se está flasheando mitad por mitad: la diferencia es esperada.
    """
    if any(PENDING_DIR.glob("sofle_*.uf2")):
        return
    with sofle_hid.Keyboard() as kb:
        other = kb.request(sofle_hid.CMD_OTHER_HALF)
        own = kb.firmware_version()
    if other[0] == sofle_hid.CMD_OTHER_HALF and other[1] == 2:
        other_version = other[2:].split(b"\0")[0].decode()
        message = f"Las mitades tienen firmware distinto: USB {own}, otra {other_version}. Flashea las dos."
        log(message)
        subprocess.run(["notify-send", "-a", "Sofle", "-u", "critical", "Sofle", message], check=False)


def latest_backup(backup_dir):
    backups = sorted(backup_dir.glob("sofle-backup-*.json"))
    return backups[-1] if backups else None


def periodic_backup(backup_dir):
    with sofle_hid.Keyboard() as kb:
        backup = sofle_hid.take_backup(kb)
    TEMP_BACKUP.write_text(json.dumps(backup, indent=1))

    previous = latest_backup(backup_dir)
    if previous is not None and sofle_hid.backup_content(json.loads(previous.read_text())) == sofle_hid.backup_content(backup):
        log(f"Respaldo sin cambios respecto a {previous.name}: no se guarda")
    else:
        backup_dir.mkdir(parents=True, exist_ok=True)
        target = backup_dir / f"sofle-backup-{datetime.datetime.now():%Y%m%d-%H%M%S}.json"
        # /tmp suele ser tmpfs: shutil.move copia si el destino está en otro sistema de archivos
        shutil.move(TEMP_BACKUP, target)
        log(f"Respaldo con cambios guardado en {target}")
    TEMP_BACKUP.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Agente del Sofle: hora y respaldos periódicos")
    parser.add_argument("--time-minutes", type=float, default=15, help="cada cuántos minutos enviar la hora (por defecto 15)")
    parser.add_argument("--backup-hours", type=float, default=6, help="cada cuántas horas revisar si hay que respaldar (por defecto 6)")
    parser.add_argument("--backup-dir", type=Path, default=sofle_hid.DEFAULT_BACKUP_DIR / "backups", help="carpeta de respaldos")
    args = parser.parse_args()
    backup_dir = args.backup_dir.expanduser()

    log(f"Agente iniciado: hora cada {args.time_minutes:g} min, respaldo cada {args.backup_hours:g} h en {backup_dir}")
    connection = None
    next_time_sync = 0.0
    next_backup = 0.0

    while True:
        current = keyboard_connection()
        if current != connection:
            connection = current
            if current is None:
                log("Teclado desconectado")
            else:
                log("Teclado conectado")
                time.sleep(SETTLE_SECONDS)
                next_time_sync = 0.0  # recién conectado o reiniciado: la hora se envía ya
                try:
                    check_halves()
                except (sofle_hid.KeyboardError, OSError) as error:
                    log(f"No se pudo comparar las mitades: {error}")

        if connection is not None:
            now = time.monotonic()
            try:
                if now >= next_time_sync:
                    sync_time()
                    next_time_sync = now + args.time_minutes * 60
                if now >= next_backup:
                    periodic_backup(backup_dir)
                    next_backup = now + args.backup_hours * 3600
            except sofle_hid.KeyboardError as error:
                # Teclado ocupado, reiniciándose o en modo de carga: se reintenta en el próximo ciclo
                log(f"No se pudo hablar con el teclado: {error}")
            except OSError as error:
                log(f"Error de archivos o del sistema: {error}")

        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("Agente detenido")
