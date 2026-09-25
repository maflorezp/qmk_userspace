# Sofle con Raspberry Pi Pico (RP2040) cableado sobre el footprint del Pro Micro.
# El converter solo aporta MCU, bootloader y drivers de RP2040; los pines reales se fijan en config.h.
CONVERT_TO = kb2040

SPLIT_KEYBOARD = yes

CONSOLE_ENABLE = yes
COMMAND_ENABLE = no
RAW_ENABLE = yes
# Configurador web usevia.app: keymap, macros y RGB editables y guardados en la EEPROM
VIA_ENABLE = yes

EXTRAKEY_ENABLE = yes
MOUSEKEY_ENABLE = yes
DYNAMIC_MACRO_ENABLE = yes
CAPS_WORD_ENABLE = yes
TRI_LAYER_ENABLE = yes
SWAP_HANDS_ENABLE = no
TAP_DANCE_ENABLE = no

ENCODER_ENABLE = yes
ENCODER_MAP_ENABLE = yes

OLED_ENABLE = yes
# Velocidad de tipeo para la pantalla
WPM_ENABLE = yes
RGBLIGHT_ENABLE = yes
# Driver propio que envuelve al ws2812 de QMK con verificación de índice (ver rgb_driver.c).
# Se puede volver al de QMK con -e RGBLIGHT_DRIVER=ws2812 cuando QMK traiga la corrección.
RGBLIGHT_DRIVER ?= custom
ifeq ($(strip $(RGBLIGHT_DRIVER)), custom)
    WS2812_DRIVER_REQUIRED = yes
    SRC += rgb_driver.c
endif

# Firmware de diagnóstico para encontrar el bus I2C de la OLED: compilar con -e I2C_SCAN=yes.
# Apaga el driver de la OLED para que no ocupe los pines mientras se prueban por software.
ifeq ($(strip $(I2C_SCAN)), yes)
    OLED_ENABLE = no
    # El escaneo mueve GP2/GP3; con el encoder activo se leerían como giros (volumen)
    ENCODER_ENABLE = no
    ENCODER_MAP_ENABLE = no
    OPT_DEFS += -DI2C_SCAN_ENABLE
    SRC += i2c_scan.c
endif

# Firmware de diagnóstico: enciende todos los píxeles de ambas OLED (compilar con -e OLED_TEST=yes)
ifeq ($(strip $(OLED_TEST)), yes)
    OPT_DEFS += -DOLED_TEST_ENABLE
endif

SRC += settings.c rgb.c hid_protocol.c version.c clock.c split_master.c

# Versión del firmware: commit del userspace y tipo de build. Con cambios sin commitear se agrega
# "+" y una huella del código del keymap, para que cada build distinto tenga una versión distinta.
BUILD_COMMIT := $(shell git -C $(KEYMAP_PATH) rev-parse --short=7 HEAD 2>/dev/null || echo nogit)
BUILD_DIRTY := $(shell git -C $(KEYMAP_PATH) status --porcelain -- . 2>/dev/null)
ifneq ($(strip $(BUILD_DIRTY)),)
    BUILD_ID := $(BUILD_COMMIT)+$(shell cat $(KEYMAP_PATH)/*.c $(KEYMAP_PATH)/*.h $(KEYMAP_PATH)/rules.mk | sha1sum | cut -c1-4)
else
    BUILD_ID := $(BUILD_COMMIT)
endif
BUILD_VARIANT := normal
ifeq ($(strip $(PIN_SCAN)), yes)
    BUILD_VARIANT := pinscan
endif
ifeq ($(strip $(I2C_SCAN)), yes)
    BUILD_VARIANT := i2cscan
endif
ifeq ($(strip $(OLED_TEST)), yes)
    BUILD_VARIANT := oledtest
endif
OPT_DEFS += -DBUILD_ID=\"$(BUILD_ID)\" -DBUILD_VARIANT=\"$(BUILD_VARIANT)\"

ifeq ($(strip $(OLED_ENABLE)), yes)
    SRC += oled.c
endif

ifeq ($(strip $(VIA_ENABLE)), yes)
    SRC += via_menu.c
endif

# Firmware de diagnóstico para encontrar los pines del encoder: compilar con -e PIN_SCAN=yes
ifeq ($(strip $(PIN_SCAN)), yes)
    OPT_DEFS += -DPIN_SCAN_ENABLE
    SRC += pin_scan.c
endif

# Graba el lado en la EEPROM al arrancar (compilar con -e HAND=left o -e HAND=right)
ifeq ($(strip $(HAND)), left)
    OPT_DEFS += -DINIT_EE_HANDS_LEFT
endif
ifeq ($(strip $(HAND)), right)
    OPT_DEFS += -DINIT_EE_HANDS_RIGHT
endif

# El lado va en el nombre del .uf2 para que las dos compilaciones no se pisen (sofle_rev1_maflorezp_left.uf2)
ifneq ($(strip $(HAND)),)
    override TARGET := $(TARGET)_$(HAND)
endif
