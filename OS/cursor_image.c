/*
 * HomeOS — disegno dell'immagine del puntatore.
 *
 * Nessun accesso all'hardware: si scrive in un buffer di pixel e basta. Cosi'
 * la forma si puo' guardare sul computer di sviluppo (tools/screen_preview)
 * prima di scrivere la microSD, invece di scoprirla sulla scheda.
 *
 * Due accorgimenti, entrambi per non dipendere da come il controller
 * interpreta i colori:
 *   - i pixel trasparenti sono zero puro, che e' anche il colore di sfondo
 *     scritto nel registro: se il controller usasse la chiave di colore
 *     invece del canale alfa, il risultato e' lo stesso;
 *   - il contorno e' un grigio molto scuro e non nero pieno, perche' il nero
 *     coinciderebbe con il trasparente in una chiave di colore.
 */

#include "cursor_image.h"

#define COL_TRANSPARENT 0x00000000u
#define COL_OUTLINE     0xFF101010u
#define COL_FILL        0xFFFFFFFFu
#define COL_SHADOW      0x80000000u

/* La forma si disegna su una griglia e ogni casella diventa SCALE x SCALE
 * pixel: due, cosi' il puntatore e' circa 26x42 pixel a schermo, la misura
 * di un cursore normale. */
#define SHAPE_SCALE 2u
#define HEAD_ROWS   12u   /* altezza della testa triangolare */
#define TAIL_ROWS   8u    /* lunghezza della coda             */
#define TAIL_WIDTH  4u    /* larghezza della coda             */

const char *cursor_shape_name(int shape) {
    switch (shape) {
        case CURSOR_SHAPE_WEDGE:          return "wedge";
        case CURSOR_SHAPE_CLASSIC:        return "classic";
        case CURSOR_SHAPE_CLASSIC_SHADOW: return "classic+shadow";
        default:                          return "?";
    }
}

void cursor_image_clear(volatile uint32_t *image) {
    for (uint32_t i = 0; i < CURSOR_IMAGE_PIXELS; i++) {
        image[i] = COL_TRANSPARENT;
    }
}

/* Un punto della griglia, ingrandito di SHAPE_SCALE. */
static void dot(volatile uint32_t *image, int gx, int gy, uint32_t color) {
    if (gx < 0 || gy < 0) return;
    for (uint32_t dy = 0; dy < SHAPE_SCALE; dy++) {
        for (uint32_t dx = 0; dx < SHAPE_SCALE; dx++) {
            uint32_t x = (uint32_t)gx * SHAPE_SCALE + dx;
            uint32_t y = (uint32_t)gy * SHAPE_SCALE + dy;
            if (x >= CURSOR_IMAGE_SIZE || y >= CURSOR_IMAGE_SIZE) continue;
            image[y * CURSOR_IMAGE_SIZE + x] = color;
        }
    }
}

void cursor_image_draw(volatile uint32_t *image, int shape) {
    const uint32_t outline = COL_OUTLINE;
    const uint32_t fill = COL_FILL;
    const int offset = (shape == CURSOR_SHAPE_CLASSIC_SHADOW) ? 1 : 0;

    if (shape == CURSOR_SHAPE_WEDGE) {
        /* Il cuneo: triangolo col vertice in alto a sinistra. */
        for (uint32_t row = 0; row < HEAD_ROWS + 1u; row++) {
            const uint32_t width = row + 1u;
            for (uint32_t col = 0; col < width; col++) {
                const uint32_t edge = (col == 0u) || (col + 1u == width) ||
                                      (row == HEAD_ROWS);
                dot(image, (int)col, (int)row, edge ? outline : fill);
            }
        }
        return;
    }

    /* L'ombra e' la stessa forma, spostata di una casella e in nero tenue. */
    const int passes = (shape == CURSOR_SHAPE_CLASSIC_SHADOW) ? 2 : 1;
    for (int pass = 0; pass < passes; pass++) {
        const int is_shadow = (shape == CURSOR_SHAPE_CLASSIC_SHADOW) && (pass == 0);
        const int dx = is_shadow ? offset : 0;
        const int dy = is_shadow ? offset : 0;
        const uint32_t edge_color = is_shadow ? COL_SHADOW : outline;
        const uint32_t body_color = is_shadow ? COL_SHADOW : fill;

        /* Testa: triangolo rettangolo, punta in alto a sinistra. */
        for (uint32_t row = 0; row <= HEAD_ROWS; row++) {
            const uint32_t width = row + 1u;
            for (uint32_t col = 0; col < width; col++) {
                const uint32_t edge = (col == 0u) || (col + 1u == width) ||
                                      (row == HEAD_ROWS);
                dot(image, (int)col + dx, (int)row + dy,
                    edge ? edge_color : body_color);
            }
        }

        /* Coda: banda che scende verso destra, attaccata sotto la testa. */
        for (uint32_t i = 0; i < TAIL_ROWS; i++) {
            const uint32_t row = HEAD_ROWS + 1u + i;
            for (uint32_t j = 0; j < TAIL_WIDTH; j++) {
                const uint32_t edge = (j == 0u) || (j + 1u == TAIL_WIDTH) ||
                                      (i + 1u == TAIL_ROWS);
                dot(image, (int)(TAIL_WIDTH + i + j) + dx, (int)row + dy,
                    edge ? edge_color : body_color);
            }
        }
    }
}
