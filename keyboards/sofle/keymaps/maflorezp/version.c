#include "maflorezp.h"
#include "transactions.h"
#include <string.h>

// Versión del firmware: commit de git + tipo de build (BUILD_ID y BUILD_VARIANT vienen de rules.mk).
// La mitad USB le pregunta la suya a la otra por el cable cada pocos segundos para detectar desfases.

#define VERSION_POLL_MS 5000

static char             other_half[VERSION_STRING_SIZE] = "";
static version_status_t status                          = VERSION_UNKNOWN;
static uint32_t         poll_timer                      = 0;
static bool             polled_once                     = false;

const char *firmware_version(void) {
    return BUILD_ID "-" BUILD_VARIANT;
}

const char *other_half_version(void) {
    return other_half;
}

version_status_t version_status(void) {
    return status;
}

static void version_slave_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    char *out = out_data;
    strncpy(out, firmware_version(), out_len - 1);
    out[out_len - 1] = '\0';
}

void version_init(void) {
    transaction_register_rpc(RPC_ID_USER_VERSION, version_slave_handler);
}

void version_task(void) {
    if (!is_keyboard_master()) {
        return;
    }
    if (polled_once && timer_elapsed32(poll_timer) < VERSION_POLL_MS) {
        return;
    }
    char reply[VERSION_STRING_SIZE] = {0};
    // Si la otra mitad no responde (cable suelto o firmware sin este mensaje) la versión queda desconocida
    if (transaction_rpc_exec(RPC_ID_USER_VERSION, 0, NULL, sizeof(reply), reply)) {
        reply[sizeof(reply) - 1] = '\0';
        memcpy(other_half, reply, sizeof(other_half));
        status = strcmp(other_half, firmware_version()) == 0 ? VERSION_MATCH : VERSION_MISMATCH;
    } else {
        other_half[0] = '\0';
        status        = VERSION_UNKNOWN;
    }
    polled_once = true;
    poll_timer  = timer_read32();
}
