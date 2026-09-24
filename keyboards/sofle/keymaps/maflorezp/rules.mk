# Sofle con Raspberry Pi Pico (RP2040) cableado sobre el footprint del Pro Micro.
# El converter solo aporta MCU, bootloader y drivers de RP2040; los pines reales se fijan en config.h.
CONVERT_TO = kb2040

SPLIT_KEYBOARD = yes

CONSOLE_ENABLE = yes
COMMAND_ENABLE = no
RAW_ENABLE = yes

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
RGBLIGHT_ENABLE = yes

SRC += oled.c rgb.c hid_protocol.c

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
