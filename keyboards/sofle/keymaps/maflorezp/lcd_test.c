#include "maflorezp.h"

// Patrón de prueba de la pantalla a color: permite validar de un vistazo el cableado, los colores y
// el encuadre.
//   · marco blanco de 1 px en los cuatro bordes -> si falta un lado, el desplazamiento del panel está mal
//   · tres franjas: roja arriba, verde al medio, azul abajo -> orden de colores e inversión
//   · cuadrado blanco en la esquina superior izquierda -> orientación (espejo o giro)

#define LCD_MARKER_SIZE 24

void lcd_test_draw(painter_device_t device) {
    uint16_t width  = qp_get_width(device);
    uint16_t height = qp_get_height(device);
    uint16_t band   = height / 3;

    qp_rect(device, 0, 0, width - 1, band - 1, HSV_RED, true);
    qp_rect(device, 0, band, width - 1, 2 * band - 1, HSV_GREEN, true);
    qp_rect(device, 0, 2 * band, width - 1, height - 1, HSV_BLUE, true);
    qp_rect(device, 0, 0, width - 1, height - 1, HSV_WHITE, false);
    qp_rect(device, 0, 0, LCD_MARKER_SIZE - 1, LCD_MARKER_SIZE - 1, HSV_WHITE, true);
}
