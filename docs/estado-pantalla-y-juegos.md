# Pantalla a color, juegos y joystick: estado para retomar

Fecha: 2026-10-20. Rama `feat/via` del fork `maflorezp/qmk_userspace`.

Este documento resume dónde quedó el trabajo para continuarlo en una sesión nueva. El diseño
completo y aprobado de la pantalla está en [pantalla-color-diseno.md](pantalla-color-diseno.md):
leerlo primero.

## Reglas de trabajo que siguen vigentes

- **No compilar hasta que Mauricio diga "compila".** `build.sh` encola el firmware y el watcher
  lo flashea solo en las dos mitades.
- Código en inglés; comentarios, documentación, commits y mensajes en español.
- Commits en español, sin marcas de IA, agregando archivos uno por uno (nunca `git add .`).
  Commit y push al cerrar cada hito.
- Diseñar y acordar antes de codificar: Mauricio pide ver y opinar primero.

## Hardware

### Pantalla

- Módulo GMT147SPI: 1,47" IPS, 172x320, controlador ST7789P3, 3,3 V. **Ya llegó.**
- Va en la mitad derecha, la del USB. **Probada y funcionando en protoboard** (2026-10-20); falta
  soldarla. La primera de las dos que llegaron vino con el vidrio quebrado: se veía estática de
  colores con la luz de fondo encendida. Con la segunda todo funcionó con el mismo cableado.
- Orientación: con los pines hacia abajo, sin girar (`QP_ROTATION_0`).
- El módulo viene sin pines soldados. El pin 1, de pad cuadrado y el de más a la izquierda, es GND.
  Orden de los pines: GND, VDD, SCL, SDA, RES, DC, CS, BL.

| Pantalla | Pico derecho | Pin físico |
|---|---|---|
| GND | GND | 13 (el 18 lo usa la OLED) |
| VDD | 3V3 (OUT) | 36 |
| SCL | GP14 | 19 |
| SDA | GP15 | 20 |
| RES | 3V3 (OUT), cable propio junto al de VDD | 36 |
| DC | GP12 | 16 |
| CS | GP13 | 17 |
| BL | GP11 | 15 |

- Va en el bus SPI1 porque los pines del SPI0 (GP18 y GP19) quedan en la columna ocupada por la
  OLED y la matriz. Así las señales caen en los pines 15 a 20, con el 3V3 del 36 y el GND del 13.
- RES no necesita GPIO: va fijo a 3V3 y el driver reinicia el panel por software al arrancar.
  Nunca dejarlo suelto, porque leería ruido.

- La OLED derecha se va a retirar. La pantalla no usa sus pines (GP16 y GP17), así que para la
  prueba da igual si sigue puesta. Sin OLED el firmware no falla: QMK ignora la OLED si no inicia.

### Rueda y joystick

- **La rueda derecha se queda** con lo que hace hoy: GP2 y GP3, y su clic en la matriz (fila 9,
  columna 5).
- **En el hueco izquierdo va el joystick de un Nintendo Switch** (Joy-Con). Antes se consideró un
  d-pad de 5 direcciones; Mauricio prefirió el joystick. El diseño propuesto está abajo y falta
  que lo confirme.

## Lo que está hecho

| Qué | Dónde | Commit |
|---|---|---|
| Arreglo de la OLED: las filas ya no se desplazan al cambiar de capa | `oled.c` | `0b123cd` |
| Intermedios de compilación en `/tmp/qmk-build` | `build.sh` | `4b433d1` |
| Diseño de la pantalla, modo ayuda, pantalla izquierda, lluvia de símbolos y juegos | `docs/pantalla-color-diseno.md` | `e05daf0` a `cff44e2` |
| Juego de carritos y emulador de la pantalla para el PC | `games/`, `tools/lcd_emulator/` | `737fa03` |
| Tetris y menú de juegos | `games/` | `e225de2` |
| Breakout, culebrita, práctica de escritura y pantalla de estado (emulador) | `games/`, `screen/`, `tools/lcd_emulator/` | este hito |
| Pantalla a color en el firmware: estado, modo juego y diagnóstico por HID | `lcd*.c`, `game_mode.c` | este hito |

### Pantalla a color en el firmware

- **Tecla `CK_DISP`** ("Pantalla" en VIA), en ADJUST sobre la P: alterna entre la OLED y la
  pantalla a color. Se recuerda al reiniciar (palabra de 32 bits del teclado en la EEPROM; `EE_CLR`
  la devuelve a la OLED).
- **Bus:** SPI1 a 125 MHz / 8 = 15,6 MHz, modo 3, desplazamiento (34, 0). Refresco del panel
  subido a 111 Hz (comando `0xC6`), porque sin la patilla TE se veían cuadros a medio pintar.
- **Pantalla de estado** (`screen/status_screen.c`, portátil como los juegos): hora grande y fecha
  con el día, capa con el color de la tira, CAPS/NUM, íconos de Win/Ctrl/Alt/Shift, REC, velocidad
  de escritura, mensaje del PC o ajuste de VIA y versión (roja si las mitades no coinciden). Solo
  redibuja las zonas cuyo dato cambió. La luz de fondo se apaga con el tiempo sin teclear de la
  pantalla en VIA.
- **Diagnóstico sin recompilar** (`lcd_probe.c` + `tools/lcd_probe.py`, protocolo HID v8):
  `pins` (niveles fijos para medir con multímetro), `init` (velocidad, modo, rotación y
  desplazamiento), `raw` (comando directo), `fill`, `bitbang` (SPI por software) y `demo` (cuadrado
  que rebota). Mientras se usa, la pantalla de estado no dibuja; `CK_DISP` dos veces la devuelve.

### Juegos en el firmware

- **Tecla `CK_GAME`** ("Juegos" en VIA), en ADJUST sobre la F. Necesita la pantalla a color elegida.
- Mientras se juega ninguna tecla llega al PC (`game_mode.c`): A/J, D/L, W/I, S/K, las flechas de
  LOWER y la rueda mueven; Espacio, Enter o el clic de la rueda son la acción; Esc vuelve al menú y,
  desde el menú, sale. ADJUST + F sale directo.
- Juegos: Carritos, Tetris, Breakout, Culebrita y Escritura.
- **Escritura:** pantalla de configuración con casillas (minúsculas, mayúsculas, números, símbolos,
  tildes) y velocidad 1 a 9; la elección se guarda. Interpreta las teclas como la distribución del
  PC, US internacional con AltGr: Shift, AltGr + vocal, AltGr + n/y/1//, y la tilde muerta.
- **Récords** en el bloque de usuario de la EEPROM, después de la configuración: 8 espacios de 32
  bits (el último guarda las preferencias de Escritura). El respaldo del watcher no los incluye.
- La fuente de 5x7 (`game_font.c`) trabaja en Latin-1: minúsculas, tildes, ñ, ü, ¿, ¡ y símbolos.

### Emulador

```
tools/lcd_emulator/run.sh                  # menú de juegos, ventana al doble de tamaño
tools/lcd_emulator/run.sh --game typing    # directo a un juego (car, tetris, breakout, snake, typing)
tools/lcd_emulator/run.sh --status         # pantalla de estado con datos inventados
tools/lcd_emulator/run.sh --script "action,wait:500,type:ñá,shot:/tmp/captura.ppm"
```

## Siguientes pasos, en orden

1. **Soldar la pantalla** con el cableado de la tabla. Hasta entonces, la OLED queda elegida.
2. **Palabras por HID** para Escritura: el PC envía palabras y una casilla las mezcla con los
   conjuntos. Falta diseñar de dónde salen las palabras y cómo se envían.
3. **Joystick**: en pausa hasta que llegue el adaptador de FPC. Mauricio avisa cuándo se arranca.
4. **Las etapas del diseño que faltan:** fuente más fina y brillo por PWM en la pantalla de estado,
   modo ayuda, pantalla izquierda, lluvia de símbolos, y Pong (necesita la pantalla izquierda).

## Joystick del Switch (propuesta, en pausa hasta que llegue el FPC)

### Contactos del flex

El flex tiene 5 contactos. Según un foro
([sudomod](https://sudomod.com/forum/viewtopic.php?p=81679)): 1 alimentación (3,3 V), 2 eje Y,
3 clic, 4 tierra y 5 eje X. El mismo foro advierte que la numeración se invierte según el lado
desde el que se mire, así que se confirma con el multímetro en resistencia, con el joystick suelto:

- Alimentación y tierra son el par con resistencia fija aunque se mueva la palanca.
- X e Y, medidas contra tierra, cambian al mover la palanca: una de lado a lado y la otra de
  arriba abajo.
- El clic solo hace contacto con tierra mientras se presiona la palanca.

Se conecta con un adaptador de FPC de 0,5 mm y 6 pines (o 5) que ya traiga el conector soldado.
Con el de 6, el flex va corrido hacia un lado y sobra un pin.

### Cableado en el Pico izquierdo

El Pico solo tiene tres entradas analógicas (GP26, GP27 y GP28) y las tres son columnas de la
matriz. Se mueven dos columnas, solo en la mitad izquierda; el firmware define columnas distintas
para la derecha con `MATRIX_COL_PINS_RIGHT`.

| Qué | Hoy | Queda en |
|---|---|---|
| Cable de la columna 2 | GP27 (pin 32) | GP5 (pin 7) |
| Cable de la columna 3 | GP26 (pin 31) | GP4 (pin 6) |
| Eje X | — | GP26 (pin 31) |
| Eje Y | — | GP27 (pin 32) |
| Tierra | — | AGND (pin 33) |
| Alimentación | — | 3V3 (pin 36) |
| Clic | — | GP11 (pin 15); comparte tierra con el joystick, así que no puede ir a la matriz |

Columnas de hoy, iguales en las dos mitades: `{ GP28, GP20, GP27, GP26, GP21, GP22 }`. En la
izquierda quedan `{ GP28, GP20, GP5, GP4, GP21, GP22 }`.

### Qué hace en cada capa

| Capa | Joystick | Clic |
|---|---|---|
| QWERTY y NUMERIC | Puntero del mouse, más rápido cuanto más se inclina | Clic izquierdo |
| LOWER | Flechas: inclinar es mantener la flecha; en diagonal, dos | Enter |
| RAISE | Desplazamiento vertical y horizontal | Clic central |

La velocidad y la zona muerta del centro se ajustan al probarlo; después pueden ir al menú propio
de VIA (`via/sofle_maflorezp.json`).

### Firmware

- Driver `analog_joystick` de QMK (`POINTING_DEVICE_ENABLE`), con `ANALOG_JOYSTICK_X_AXIS_PIN`,
  `ANALOG_JOYSTICK_Y_AXIS_PIN` y `ANALOG_JOYSTICK_CLICK_PIN`. Lee el centro al arrancar, así que
  la palanca tiene que estar suelta al conectar el teclado.
- `SPLIT_POINTING_ENABLE` con `POINTING_DEVICE_LEFT`: el joystick está en la izquierda y el USB
  entra por la derecha, así que sus datos pasan por el cable entre mitades.
- Archivo nuevo `joystick.c`, con `pointing_device_task_user`, para los modos de cada capa y el clic.
- La placa `QMK_PM2040` ya trae el ADC activado (`HAL_USE_ADC`, `RP_ADC_USE_ADC1`), y el
  `halconf.h` y el `mcuconf.h` propios del keymap no lo tocan: solo cambian el I2C de la OLED.
- VIA: quitar la rueda izquierda que se dibuja en el hueco (`e0`, posición 4,5); nunca se instaló.

### Orden

1. Mover los dos cables de columna y flashear un firmware con las columnas nuevas, sin el joystick.
   Comprobar que respondan todas las teclas. Cable y firmware van juntos: mientras no coincidan,
   las teclas de esas dos columnas no funcionan.
2. Conectar el joystick, flashear con él activado y ajustar sentido y velocidad.

### Cambios pendientes en el diseño de la pantalla

Cuando se confirme, actualizar `pantalla-color-diseno.md`:

- En los juegos, el control pasa a ser el joystick o la rueda: las direcciones mueven y el clic
  entra o gira.
- En Pong, la raqueta izquierda sigue al joystick de forma proporcional y la derecha va con la rueda.
- En el modo ayuda, también se muestra qué hace el joystick en cada capa.

## Materiales para comprar

- Adaptador de FPC de 0,5 mm y 6 pines con el conector soldado, para el flex del joystick.
- Cable de silicona de 30 AWG para soldar dentro del teclado. Mauricio encontró un kit BOJACK
  con 5 colores, termoencogible de 2 mm y pelacables. Hay que estañar las puntas, pelar 2 a 3 mm
  y fijar el recorrido con cinta Kapton. Otra opción es el de wire wrap Kynar de 30 AWG, que
  conserva la forma que se le da.
- Cinta Kapton.
- Para el protoboard: cables Dupont (MM, HM, HH) o cable rígido de 24 AWG. El de 30 AWG queda
  flojo en el protoboard.

## Pendientes de antes

- Capa para ajustar la hora y la fecha desde el teclado.
- Unir `feat/via` en `main`.
- Que el watcher maneje los dos USB a la vez.
