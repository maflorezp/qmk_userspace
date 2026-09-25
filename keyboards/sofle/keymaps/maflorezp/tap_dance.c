#include "maflorezp.h"

// Teclas de modificadores dobles (encima de cada Shift):
//   toque         -> one-shot de sus modificadores (se aplican a la siguiente tecla)
//   mantener      -> sus modificadores mientras se mantiene
//   doble toque   -> one-shot de Ctrl+Alt+Shift (igual en las dos teclas)
//   doble toque y mantener -> los modificadores de la otra tecla mientras se mantiene
// Izquierda: Ctrl+Shift (inverso Ctrl+Alt). Derecha: Ctrl+Alt (inverso Ctrl+Shift).
// El arreglo tap_dance_actions va en keymap.c: QMK lo busca ahí para validar el keymap.

void dual_mods_finished(tap_dance_state_t *state, void *user_data) {
    dual_mods_t *dual         = user_data;
    bool         double_press = state->count >= 2;
    if (state->pressed) {
        dual->held_mods = double_press ? dual->inverse_mods : dual->mods;
        register_mods(dual->held_mods);
    } else {
        add_oneshot_mods(double_press ? MODS_CTRL_ALT_SHIFT : dual->mods);
    }
}

void dual_mods_reset(tap_dance_state_t *state, void *user_data) {
    dual_mods_t *dual = user_data;
    if (dual->held_mods) {
        unregister_mods(dual->held_mods);
        dual->held_mods = 0;
    }
}
