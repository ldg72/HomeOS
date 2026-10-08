#ifndef HOMEOS_CURSOR_IMAGE_H
#define HOMEOS_CURSOR_IMAGE_H

#include <stdint.h>

/*
 * Disegno dell'immagine del puntatore.
 *
 * Separato dai registri apposta: qui non c'e' nessun accesso MMIO, quindi lo
 * stesso codice che gira sulla scheda si compila anche sul computer di
 * sviluppo e si puo' guardare la forma prima di scrivere la microSD.
 *
 * Il piano cursore del DC8200 accetta ARGB8888 fino a 64x64. La forma sta in
 * alto a sinistra e la punta e' il pixel (0,0), che e' anche il punto caldo.
 */

#define CURSOR_IMAGE_SIZE   64u
#define CURSOR_IMAGE_PIXELS (CURSOR_IMAGE_SIZE * CURSOR_IMAGE_SIZE)
#define CURSOR_IMAGE_BYTES  (CURSOR_IMAGE_PIXELS * 4u)

enum cursor_shape {
    CURSOR_SHAPE_WEDGE = 0,       /* il cuneo: triangolo senza coda      */
    CURSOR_SHAPE_CLASSIC,         /* freccia con la coda                 */
    CURSOR_SHAPE_CLASSIC_SHADOW,  /* la stessa, con l'ombra sotto        */
    CURSOR_SHAPE_COUNT
};

const char *cursor_shape_name(int shape);

void cursor_image_clear(volatile uint32_t *image);
void cursor_image_draw(volatile uint32_t *image, int shape);

#endif
