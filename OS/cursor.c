/*
 * HomeOS — cursore hardware del DC8200.
 *
 * I valori dei registri non sono dedotti: vengono dal driver di riferimento
 * (drivers/gpu/drm/verisilicon/vs_dc_hw.c), che e' la stessa fonte da cui e'
 * stata portata la sequenza del piano primario. Due cose restano da provare
 * sulla scheda e sono esposte come tali:
 *
 *   - la codifica del campo "size" (tre bit, 32x32 o 64x64);
 *   - se il cursore vada acceso anche con il clock gating (0x1484): il driver
 *     non lo tocca, quindi presumibilmente il gate e' aperto di serie.
 *
 * Del driver si copia anche la stranezza: in scrittura i bit bassi valgono
 * 0x0E, in spegnimento si azzerano i bit 0-1. Non sapendo cosa siano, si fa
 * la stessa cosa che fa lui.
 */

#include <stddef.h>
#include <stdint.h>

#include "cursor.h"
#include "cursor_image.h"
#include "display.h"
#include "mmio.h"

#define DC 0x29400000UL

/* Framebuffer riservato dal Core: il DC legge dal fisico, la CPU scrive
 * attraverso l'alias non cachato. Vale anche per l'immagine del cursore. */
#define MARSFB_PHYS     0x70000000UL
#define MARSFB_UNCACHED 0x470000000UL
#define MARSFB_BYTES    (1920u * 1080u * 4u)

/* Registri del cursore (dal driver di riferimento). */
#define DC_CURSOR_CONFIG     0x1468u
#define DC_CURSOR_ADDRESS    0x146Cu
#define DC_CURSOR_LOCATION   0x1470u
#define DC_CURSOR_BACKGROUND 0x1474u
#define DC_CURSOR_FOREGROUND 0x1478u
#define DC_CURSOR_CLK_GATING 0x1484u

/* Bit del registro di configurazione. */
#define CURSOR_CFG_ENABLE_BITS 0x0Eu   /* bit 1, 2, 3: come nel driver        */
#define CURSOR_CFG_SIZE_SHIFT  5u
#define CURSOR_CFG_SIZE_MASK   0x07u
#define CURSOR_CFG_CLEAR_MASK  0x00FFFFFFu
/* Spegnimento, come nel driver di riferimento. */
#define CURSOR_CFG_OFF_SET     0x08u   /* bit 3 */
#define CURSOR_CFG_OFF_CLEAR   0x03u   /* bit 0 e 1 */
static uint32_t g_image_offset;
static uint32_t g_image_phys;
static uint32_t g_placed_width;    /* risoluzione a cui l'immagine e' collocata */
static uint32_t g_placed_height;
static uint32_t g_size_code = 1u;   /* si prova con 64x64 */
static int g_shape = CURSOR_SHAPE_CLASSIC;
static int g_ready;
static int g_enabled;
static uint16_t g_x, g_y;

/* --------------------------------------------------------------- registri */

/*
 * La configurazione si scrive sempre nella forma "acceso": spegnere il
 * cursore da qui non funziona su questa scheda (vedi cursor_paint_image), e
 * riscrivere questo registro e' comunque necessario a ogni spostamento,
 * perche' e' cosi' che il blocco ricarica la posizione.
 */
static void cursor_write_config(int enabled) {
    uint32_t set = ((g_size_code & CURSOR_CFG_SIZE_MASK) << CURSOR_CFG_SIZE_SHIFT) |
                   CURSOR_CFG_ENABLE_BITS;
    uint32_t clear = CURSOR_CFG_CLEAR_MASK;
    uint32_t current = mmio_read32(DC + DC_CURSOR_CONFIG);

    if (!enabled) {
        /* Come il driver di riferimento: azzera i bit 0-1 e lascia alto il
         * bit 3. Da solo questo non nasconde il puntatore, e va bene: quando
         * si arriva qui il cursore e' gia' stato portato fuori dallo
         * schermo. */
        set = CURSOR_CFG_OFF_SET;
        clear = CURSOR_CFG_OFF_CLEAR;
    }

    mmio_write32(DC + DC_CURSOR_CONFIG, (current & ~clear) | set);
}

static void cursor_write_location(int enabled) {
    /*
     * Spento: la posizione va fuori dallo schermo. E' il modo piu' semplice ed
     * economico di nascondere il puntatore, ma da solo non basta: va scritto
     * mentre la configurazione e' ancora nella forma "acceso", perche' e' la
     * sua riscrittura che fa applicare la posizione. E' il primo dei due passi
     * di cursor_enable(0). La posizione vera la tiene il software, quindi
     * riaccendendo il puntatore torna dov'era.
     */
    const uint32_t x = enabled ? (uint32_t)g_x : 0xFFFFu;
    const uint32_t y = enabled ? (uint32_t)g_y : 0xFFFFu;

    mmio_write32(DC + DC_CURSOR_LOCATION, x | (y << 16));
}

/*
 * Scrive indirizzo, posizione e configurazione — la stessa sequenza del
 * driver di riferimento.
 *
 * Non e' ridondanza: scritta da sola, la posizione non ha effetto. La scheda
 * lo ha detto in modo netto: dando le coordinate il cursore non si muoveva,
 * e si spostava solo al successivo comando che riscriveva la configurazione.
 * Evidentemente il blocco del cursore ricarica le sue copie di lavoro quando
 * si riscrive il registro di configurazione, quindi ogni spostamento deve
 * passare da qui. Era una "ottimizzazione" mia, ed era sbagliata.
 */
static void cursor_write_registers(int enabled) {
    mmio_write32(DC + DC_CURSOR_ADDRESS, g_image_phys);
    cursor_write_location(enabled);
    cursor_write_config(enabled);
}

/*
 * Colloca l'immagine per la risoluzione corrente e la disegna.
 * Va rifatta quando la risoluzione cambia: l'immagine sta subito dopo lo
 * schermo visibile, e ingrandendo lo schermo quel posto finisce dentro i
 * pixel visibili — e' il quadrato nero comparso a 1080p, dove il controller
 * leggeva come cursore un pezzo di console.
 */
static int cursor_place_image(void) {
    const uint32_t width = display_width();
    const uint32_t height = display_height();
    uint32_t offset;

    if (width == 0u || height == 0u) return CURSOR_ERR_NO_DISPLAY;

    offset = (width * height * 4u + 4095u) & ~4095u;
    if (offset + CURSOR_IMAGE_BYTES > MARSFB_BYTES) return CURSOR_ERR_NO_ROOM;

    g_image_offset = offset;
    g_image_phys = MARSFB_PHYS + offset;
    g_placed_width = width;
    g_placed_height = height;
    return CURSOR_OK;
}

/* Ridisegna l'immagine con la forma corrente. Si fa quando cambia lo stato o
 * la forma, non a ogni spostamento: sono 16 KiB di scritture non cachate. */
static void cursor_paint_image(void) {
    volatile uint32_t *image =
        (volatile uint32_t *)(uintptr_t)(MARSFB_UNCACHED + g_image_offset);

    cursor_image_clear(image);
    cursor_image_draw(image, g_shape);
    mmio_fence();   /* l'immagine deve essere in memoria prima che la legga */
}

/* Applica lo stato corrente, rifacendo la collocazione se serve. */
static void cursor_apply(void) {
    if (!g_ready) return;

    if (display_width() != g_placed_width ||
        display_height() != g_placed_height) {
        if (cursor_place_image() != CURSOR_OK) {
            /* Non c'e' piu' posto (1080p): si spegne invece di mostrare
             * spazzatura, e il comando lo dira'. */
            g_enabled = 0;
            g_ready = 0;
            return;
        }
        cursor_paint_image();
        if (g_x >= display_width()) g_x = (uint16_t)(display_width() - 1u);
        if (g_y >= display_height()) g_y = (uint16_t)(display_height() - 1u);
    }

    cursor_write_registers(g_enabled);
}

/* -------------------------------------------------------------- interfaccia */

int cursor_init(void) {
    int rc;

    g_ready = 0;
    g_enabled = 0;

    rc = cursor_place_image();
    if (rc != CURSOR_OK) return rc;

    mmio_write32(DC + DC_CURSOR_BACKGROUND, 0x00000000u);
    mmio_write32(DC + DC_CURSOR_FOREGROUND, 0x00FFFFFFu);

    g_x = (uint16_t)(display_width() / 2u);
    g_y = (uint16_t)(display_height() / 2u);

    g_ready = 1;
    g_enabled = 1;
    cursor_paint_image();
    cursor_write_registers(g_enabled);
    return CURSOR_OK;
}

void cursor_move(int32_t x, int32_t y) {
    const int32_t width = (int32_t)display_width();
    const int32_t height = (int32_t)display_height();

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (width > 0 && x > width - 1) x = width - 1;
    if (height > 0 && y > height - 1) y = height - 1;

    g_x = (uint16_t)x;
    g_y = (uint16_t)y;
    if (g_ready) cursor_apply();
}

void cursor_enable(int on) {
    if (!g_ready) return;

    if (on) {
        g_enabled = 1;
        cursor_write_registers(1);   /* la forma e' gia' nell'immagine */
        return;
    }

    /*
     * Spegnere, e l'ordine conta:
     *
     *  1. la posizione va fuori dallo schermo con la configurazione ancora
     *     accesa. E' la riscrittura della configurazione che fa applicare la
     *     posizione, quindi il puntatore esce davvero;
     *  2. poi si spegne il blocco, che resta congelato la' fuori.
     *
     * Al contrario — spegnere e basta — il meccanismo che applica la
     * posizione si ferma e il puntatore resta congelato sullo schermo, dove
     * era: il comando dice "spento" e l'immagine e' ancora li'.
     */
    cursor_write_location(0);   /* fuori dallo schermo */
    cursor_write_config(1);     /* ...e adesso la posizione viene applicata */
    g_enabled = 0;
    cursor_write_config(0);     /* solo ora si spegne il blocco */
}

int cursor_is_enabled(void) { return g_enabled; }
int cursor_is_ready(void) { return g_ready; }

void cursor_set_size_code(uint32_t code) {
    g_size_code = code & CURSOR_CFG_SIZE_MASK;
    if (g_ready) cursor_apply();
}

uint32_t cursor_size_code(void) { return g_size_code; }

void cursor_set_shape(int shape) {
    if (shape < 0 || shape >= CURSOR_SHAPE_COUNT) return;
    g_shape = shape;
    if (g_ready) {
        cursor_paint_image();
        cursor_write_registers(g_enabled);
    }
}

int cursor_shape(void) { return g_shape; }

void cursor_position(uint16_t *x, uint16_t *y) {
    if (x) *x = g_x;
    if (y) *y = g_y;
}

uint32_t cursor_image_phys(void) { return g_image_phys; }
uint32_t cursor_image_offset(void) { return g_image_offset; }

void cursor_diag(uint32_t *config, uint32_t *clk_gating) {
    if (config) *config = mmio_read32(DC + DC_CURSOR_CONFIG);
    if (clk_gating) *clk_gating = mmio_read32(DC + DC_CURSOR_CLK_GATING);
}
