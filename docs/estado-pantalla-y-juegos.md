# Pantalla a color, juegos y joystick: estado para retomar

Fecha: 2026-10-18. Rama `feat/via` del fork `maflorezp/qmk_userspace`.

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
- Va en la mitad derecha, la del USB. Mauricio la va a probar primero en un protoboard, con
  cables Dupont, antes de instalarla.
- El módulo viene sin pines soldados. El pin 1, de pad cuadrado y el de más a la izquierda, es GND.
  Orden de los pines: GND, VDD, SCL, SDA, RES, DC, CS, BL.

| Pantalla | Pico derecho | Pin físico |
|---|---|---|
| GND | GND | 18 |
| VDD | 3V3 (OUT) | 36 |
| SCL | GP18 | 24 |
| SDA | GP19 | 25 |
| RES | GP15 | 20 |
| DC | GP14 | 19 |
| CS | GP13 | 17 |
| BL | GP12 | 16 |

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

### Sin commitear: prueba de encendido de la pantalla

Escrita pero sin compilar ni probar. Se commitea cuando se verifique en el hardware.

- `lcd_test.c` (nuevo): dibuja un marco blanco de 1 px, tres franjas (roja, verde y azul) y un
  cuadrado blanco de 24 px arriba a la izquierda. Se redibuja cada 3 s, así que se recupera si un
  cable hizo falso contacto al arrancar. SPI a 125 MHz / 16 = 7,8 MHz, lento a propósito para los
  cables del protoboard; modo SPI 3; desplazamiento del panel (34, 0).
- `config.h`: SPI0 en GP18 y GP19, sin MISO, y los pines CS, DC, RES y BL.
- `rules.mk`: con `-e LCD_TEST=yes` activa Quantum Painter con `st7789_spi` y la variante
  `lcdtest`.
- `keymap.c` y `maflorezp.h`: llaman a `lcd_test_init()` y `lcd_test_task()` bajo
  `#ifdef LCD_TEST_ENABLE`.
- `build.sh`: opción `./build.sh lcdtest`.

## Juegos

Corren igual en el teclado y en el PC: solo dependen de `games/game_host.h`, que ofrece pintar
rectángulos con color HSV y guardar o leer el récord de cada juego. En el teclado lo va a
implementar Quantum Painter; en el PC, el emulador con SDL2. Todavía no entran en el firmware.

| Archivo | Qué hace |
|---|---|
| `game_host.h` | Interfaz con la pantalla: 172x320, colores, entradas (izquierda, derecha, arriba, abajo, acción, atrás) y récords |
| `game_font.c` | Letras de 5x7 dibujadas con rectángulos (0-9, A-Z, `:`, `-`, `!`) |
| `game_blocks.c` | Tablero de cuadros estilo Brick Game; solo redibuja los cuadros que cambian, para ahorrar SPI |
| `game_ui.c` | Marcador superior y recuadro de mensajes |
| `game_car.c` | Carritos: 3 carriles, olas de 1 o 2 carros, sube de nivel cada 8 puntos |
| `game_tetris.c` | Tetris: tablero de 10x20 en cuadros de 14 px, bolsa de 7 piezas y vista de la siguiente |
| `game_system.c` | Menú "JUEGOS" con el récord de cada juego; Esc vuelve al menú y, desde el menú, sale |

### Emulador

```
tools/lcd_emulator/run.sh                 # ventana al doble de tamaño
tools/lcd_emulator/run.sh --game tetris   # directo a un juego (car o tetris)
tools/lcd_emulator/run.sh --script "action,wait:500,left,shot:/tmp/captura.ppm"
```

- Controles: las flechas o la rueda del mouse hacen de flechas y de rueda; espacio, Enter o clic
  hacen de clic; Esc vuelve; Q sale.
- El modo `--script` corre sin ventana con tiempo simulado. Sirve para verificar con capturas
  (PPM; se pasan a PNG con PIL) e informa cuántos píxeles se pintan por paso.
- Compila con `-Wall -Wextra -Werror` en `/tmp/qmk-build/lcd_emulator`.
- Riesgo conocido: en el peor caso, el carrito pinta unos 13.600 píxeles por paso, alrededor de
  30 ms de SPI, y el paso más rápido es de 50 ms. Va justo; optimizar si en el hardware se nota.

## Siguientes pasos, en orden

1. **Prueba de encendido**, cuando Mauricio diga "compila".
   - `./build.sh lcdtest` compila el firmware normal **y lo encola en las dos mitades**. El
     firmware de prueba queda aparte en `firmware/sofle_lcdtest-<versión>.uf2`, **sin encolar**.
   - Hay que cargarlo a mano en la derecha con `./flash.sh firmware/sofle_lcdtest-<versión>.uf2
     right`. **Antes de compilar, revisar que el watcher no lo pise** con el normal que quedó en
     cola, porque él flashea lo pendiente en cuanto ve la mitad en modo de carga.
   - Qué indica el patrón:
     - si falta un lado del marco, el desplazamiento está mal;
     - si los colores salen cambiados o invertidos, falla el orden RGB/BGR o la inversión;
     - si sale en espejo o girado, está mal la rotación.
   - Si pasa, commitear la prueba.
2. **Breakout**, que es el siguiente juego, y luego la culebrita. Pong y la práctica de escritura
   necesitan la pantalla izquierda.
3. **Joystick**, cuando Mauricio confirme el diseño y tenga los materiales.
4. **Las etapas del diseño:** pantalla normal, modo ayuda, pantalla izquierda, lluvia de símbolos
   y los juegos dentro del firmware (`CK_GAME` y récords en la memoria del Pico).

## Joystick del Switch (propuesta, falta confirmar)

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
