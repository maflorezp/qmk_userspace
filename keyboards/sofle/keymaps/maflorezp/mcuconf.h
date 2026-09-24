#pragma once

#include_next <mcuconf.h>

// La OLED está en GP16/GP17, que pertenecen al periférico I2C0 (la placa base del converter usa I2C1)
#undef RP_I2C_USE_I2C0
#define RP_I2C_USE_I2C0 TRUE
#undef RP_I2C_USE_I2C1
#define RP_I2C_USE_I2C1 FALSE
