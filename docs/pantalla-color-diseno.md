# Pantalla a color del Sofle: diseño

Fecha: 2026-10-08. Estado: aprobado en conversación, pendiente de revisión escrita.

## Objetivo

Reemplazar las OLED de 128x32 por una pantalla a color en la mitad derecha, que muestre el estado
del teclado de forma más legible y agregue un modo de ayuda que diga qué hace cada tecla en cada capa.

## Hardware

- Módulo GMT147SPI: 1.47" IPS, 172x320, controlador ST7789P3, 3.3 V.
- Va en la mitad **derecha** (la del USB): recibe la hora y los mensajes sin pasar por el enlace
  entre mitades y ve las teclas de todo el teclado.
- Cableado al Pico (bus SPI0):

| Pantalla | Pico |
|---|---|
| GND | GND |
| VDD | 3V3 |
| SCL | GP18 |
| SDA | GP19 |
| RES | GP15 |
| DC | GP14 |
| CS | GP13 |
| BL | GP12 |

- El controlador maneja 240 columnas y el panel usa las 172 centrales: desplazamiento (34, 0).
- La OLED derecha se retira; la izquierda (dañada) se desactiva en el firmware.

## Pantalla normal

De arriba hacia abajo, en vertical:

1. Hora grande (los ":" parpadean) y fecha. Sin hora del PC: `--:--`.
2. Capa actual, bloque pequeño con el color de la capa (el mismo de la tira LED) y nombre completo.
3. Bloqueos (CAPS, NUM) y modificadores, **siempre dibujados**: atenuados (gris) cuando están
   inactivos y encendidos cuando están activos. Los modificadores son íconos, no letras:
   Win ⊞ (cuatro cuadros), Ctrl ⌃, Alt ⌥, Shift ⇧.
4. REC, atenuado en reposo y rojo parpadeando al grabar una macro; velocidad de escritura siempre
   visible.
5. Mensaje del PC por HID, hasta 2 líneas.
6. Versión y USB. La versión en rojo si la otra mitad no coincide o no responde.

No se muestra el estado del micrófono. Cada dato tiene posición fija: nada se desplaza.

La luz de fondo se apaga por inactividad con el tiempo que hoy usa la OLED (ajuste de VIA) y su
brillo sale del ajuste de brillo de pantalla, por PWM en GP12. Se eliminan las animaciones contra el
quemado (culebrita y Pac-Man) y sus ajustes, porque este panel no se quema.

## Modo ayuda

- **Entrada:** tecla propia `CK_HELP` ("Ayuda"), disponible en VIA para ubicarla donde se quiera.
  De fábrica en ADJUST (LOWER + RAISE), en la posición de la letra A.
- **Mientras está activo, ninguna tecla llega al PC**: todas solo informan, incluida `CK_HELP`.
- **Salida:** solo por tiempo, 10 s sin oprimir nada (ajustable en VIA). Cada tecla reinicia la cuenta.
- **Pantalla:**

```
┌──────────────┐
│ ? 7        5 │  ícono, segundos que faltan, tecla oprimida (su valor en QWERTY)
│▀▀▀▀▀▀▀▀▀     │  barra que se acorta con el tiempo
├──────────────┤
│▌QWE        5 │
│▌LOW      ⌃⊞M │  una fila por capa, en orden, con la franja
│▌RAI        — │  y el texto del color de la capa
│▌NUM        — │
│▌ADJ        — │
│▌RGB        — │
└──────────────┘
```

- Nombres de capa en 3 letras: QWE, LOW, RAI, NUM, ADJ, RGB.
- Lo que hace la tecla se lee del keymap dinámico (lo guardado en la EEPROM, incluidos los cambios
  de VIA), no de una tabla fija.
- Una tabla convierte cada código en un nombre corto (`Vol+`, `F11`, `→ NUM`). Los modificadores
  usan los mismos íconos de la pantalla normal: Ctrl+Win+M se muestra como `⌃⊞M`. Tecla
  vacía o transparente: `—`. Código sin nombre: su valor en hexadecimal.
- El encoder también informa: giro y clic muestran lo que hacen en cada capa.

## Pantalla izquierda (segunda etapa)

Segundo módulo igual en la mitad izquierda, con el mismo cableado. Esa mitad no habla con el PC:
todo lo que muestre le llega de la derecha por el enlace entre mitades (RPC de usuario).

- **Contenido:** hora grande y fecha fijas arriba, mensajes del PC y, debajo, la lluvia de símbolos.
- **Reparto:** con las dos pantallas, la derecha queda solo con el estado del teclado.
- **Datos nuevos por el enlace:** hora, mensaje, y cada tecla oprimida (posición en la matriz).

### Lluvia de símbolos

Cada tecla oprimida suelta un símbolo que cae desde arriba con una estela que se desvanece, como en
Matrix. No bloquea el teclado ni necesita un modo aparte.

- Caen **símbolos, nunca la letra real**: quien mire la pantalla no puede leer lo que se escribe.
- Cada tecla tiene asignado su propio grupo de símbolos y al oprimirla cae uno del grupo al azar.
- Las teclas que más se usan tienen grupos más grandes (las vocales y el espacio, 4 o 5 símbolos;
  las raras, 1), así la lluvia es variada justo donde más se escribe y un símbolo repetido no
  delata a la letra más frecuente. El reparto parte de la frecuencia de letras en español.
- La asignación se baraja en cada arranque del teclado.
- La columna de caída sale de la posición de la tecla (izquierda del teclado, izquierda de la pantalla).
- La cabeza de la gota va en blanco; la estela, en el color de la capa activa (verde en QWERTY).
- Al dejar de escribir la lluvia se acaba sola y queda la hora limpia.
- Extras a definir al verla: racha mientras no se deja de escribir y récord de velocidad del día.

## Juegos (última etapa)

Versiones propias de la mecánica, no emulación de los originales.

- **Modo juego:** tecla propia `CK_GAME` en ADJUST. Abre un menú; la rueda elige y su clic entra.
  Mientras dura, ninguna tecla llega al PC. `Esc` vuelve al menú y, desde el menú, sale.
- La simulación corre en la mitad derecha; si el juego usa la pantalla izquierda, la derecha le
  envía el estado por el enlace entre mitades.
- Récord por juego guardado en la memoria del Pico.

| Orden | Juego | Pantallas | Control |
|---|---|---|---|
| 1 | Carritos (esquivar por carriles, estilo Brick Game) | una | rueda |
| 2 | Tetris (tablero 10 x 20, cuadros de 16 px) | una | flechas |
| 3 | Breakout | una | rueda como raqueta |
| 4 | Culebrita | una | flechas |
| 5 | Pong | **las dos**: cada pantalla es media cancha y la bola cruza de una a otra | una mano por raqueta; sirve para dos personas o una sola |
| 6 | Práctica de escritura | una o las dos | todo el teclado |

Carritos, Tetris y Breakout comparten el dibujo de cuadros. Pong y la práctica de escritura
dependen de la pantalla izquierda (etapa 4).

**Práctica de escritura:** caen letras y hay que oprimir cada una antes de que toque el fondo; al
acertar, desaparece. Sube de velocidad con los aciertos y lleva precisión y letras por minuto.
Niveles: fila central, todas las letras, números y símbolos (que exigen cambiar de capa, así
también se practican las capas). Lo que cae se toma del keymap real, incluido lo cambiado en VIA.

## Componentes

- `lcd.c`: inicio del panel (Quantum Painter, `st7789_spi`), luz de fondo y apagado por inactividad.
  Decide qué pantalla dibujar: aviso de VIA o mensaje, ayuda, o estado.
- `lcd_status.c`: pantalla normal. Redibuja solo las zonas cuyo dato cambió.
- `lcd_help.c`: modo ayuda (estado, cuenta regresiva, bloqueo de teclas en `process_record_user`).
- `keycode_names.c`: código de tecla → nombre corto. Sin dependencia de la pantalla.
- Fuentes de Quantum Painter generadas con `qmk painter-convert-graphics` / `painter-make-font`:
  una grande para la hora y una mediana para el resto. Los íconos (ayuda "?", Win, Ctrl, Alt,
  Shift) van como glifos adicionales de la fuente mediana, para poder mezclarlos con texto.
- `oled.c` y las animaciones se eliminan; `lcd_test.c` queda como diagnóstico (`./build.sh lcdtest`).
- Ajustes: se quitan los de animación y se agrega el tiempo de la ayuda. Cambia la estructura, así
  que sube `SETTINGS_MAGIC` (se restablecen los ajustes y el keymap de VIA; el flasheo los restaura
  del respaldo).

## Errores y límites

- Si el panel no responde no hay forma de saberlo (SPI de solo escritura): el teclado sigue normal.
- La mitad izquierda compila el mismo firmware; sin panel conectado, dibujar no tiene efecto.
- La hora y los mensajes dependen del agente del PC, como hoy.

## Pruebas

1. `./build.sh lcdtest`: marco blanco completo, franjas roja/verde/azul, cuadrado blanco arriba a
   la izquierda. Valida cableado, colores, orientación y desplazamiento.
2. Pantalla normal: cada dato en hardware (capas, bloqueos, modificadores, REC, mensaje por HID,
   versión distinta) y apagado por inactividad.
3. Ayuda: nombres contra el keymap leído por HID; nada llega al PC (captura de hidraw); salida a
   los 10 s; cambio de una tecla en VIA reflejado sin reflashear.

## Orden de trabajo

1. Prueba de encendido. 2. Pantalla normal. 3. Modo ayuda. 4. Pantalla izquierda con hora y
mensajes. 5. Lluvia de símbolos. 6. Juegos, en el orden de su tabla.
