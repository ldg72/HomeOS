/*
 * HomeOS — schermata di avvio.
 *
 * Composizione: sfondo scuro sfumato, un marchio geometrico (badge
 * arrotondato con la H), il nome, e in piccolo architettura, scheda, Core e
 * versione. Tutto disegnato con primitive nostre: nessuna risorsa esterna,
 * nessun font di sistema.
 *
 * Le quote sono frazioni dell'altezza, non pixel fissi: 480, 720 e 1080 righe
 * danno lo stesso disegno, riempito allo stesso modo. I testi usano fattori
 * interi — il font resta nitido solo a multipli interi — quindi la scala si
 * sceglie a scaglioni di altezza.
 */

#include "splash.h"
#include "display.h"
#include "fbcon.h"
#include "gfx.h"
#include "theme.h"
#include "version.h"

#define COL_BG_TOP   0xFF131A22u
#define COL_BG_BOT   0xFF05070Au
#define COL_ACCENT   0xFF37D2F0u
#define COL_ACCENT2  0xFF2B6CF6u
#define COL_WHITE    0xFFFFFFFFu
#define COL_TEXT     0xFFCFE6F2u
#define COL_MUTED    0xFF7F8C9Bu
#define COL_FOOT     0xFF6B7A8Cu
#define COL_GLOW     0xFF1B3A4Bu

static uint32_t scale_between(uint32_t value, uint32_t low, uint32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

void splash_compose(volatile uint32_t *buffer, uint32_t w, uint32_t h) {
    gfx_init(buffer, w, h);

    /* Sfondi e quote: tutto in frazioni di h. */
    gfx_vgradient(0, 0, w, h, COL_BG_TOP, COL_BG_BOT);

    /*
     * Scale dei testi, a scaglioni:
     *   480  ->  2 / 1 / 1        720  ->  3 / 2 / 1        1080 -> 4 / 3 / 2
     */
    const uint32_t title_scale = scale_between(h / 240u, 2u, 4u);
    const uint32_t sub_scale   = scale_between(h / 360u, 1u, 3u);
    const uint32_t small_scale = scale_between(h / 540u, 1u, 2u);

    /* Marchio: badge arrotondato con sfumatura ciano -> blu. */
    const uint32_t badge   = h / 5u;
    const uint32_t radius  = badge / 5u;
    const uint32_t ring    = scale_between(badge / 40u, 2u, 5u);
    const uint32_t bx      = (w - badge) / 2u;
    const uint32_t by      = h * 15u / 100u;

    /* Un alone appena accennato stacca il badge dal fondo. */
    gfx_rounded_rect(bx - ring, by - ring, badge + 2u * ring, badge + 2u * ring,
                     radius + ring, COL_GLOW);
    gfx_rounded_gradient(bx, by, badge, badge, radius, COL_ACCENT, COL_ACCENT2);

    /* La H, in negativo dentro il badge. */
    const uint32_t stroke  = badge / 6u;
    const uint32_t margin  = badge * 22u / 100u;
    const uint32_t arm_y   = by + badge * 25u / 100u;
    const uint32_t arm_h   = badge * 50u / 100u;
    const uint32_t right_x = bx + badge - margin - stroke;

    gfx_fill_rect(bx + margin, arm_y, stroke, arm_h, COL_WHITE);
    gfx_fill_rect(right_x, arm_y, stroke, arm_h, COL_WHITE);
    gfx_fill_rect(bx + margin, by + badge / 2u - stroke / 2u,
                  badge - 2u * margin, stroke, COL_WHITE);

    /* Nome, grande, con sfumatura bianco -> ciano. */
    gfx_text_centered_gradient(h * 42u / 100u, "HomeOS",
                              COL_WHITE, COL_ACCENT, title_scale);

    /* Riga di separazione. */
    gfx_fill_rect((w - h * 28u / 100u) / 2u, h * 58u / 100u,
                  h * 28u / 100u, scale_between(h / 240u, 2u, 4u), COL_ACCENT);

    /* Macchina e architettura. */
    gfx_text_centered(h * 62u / 100u, "RISC-V 64-bit", COL_TEXT, sub_scale);
    gfx_text_centered(h * 68u / 100u, HOMEOS_BOARD, COL_MUTED, sub_scale);

    /* Core sottostante e versione: le due righe piu' minute. */
    gfx_text_centered(h * 75u / 100u,
                      "running on " HOMEOS_CORE_NAME " " HOMEOS_CORE_VERSION,
                      COL_MUTED, small_scale);
    gfx_text_centered(h * 83u / 100u,
                      "HomeOS " HOMEOS_VERSION " \"" HOMEOS_CODENAME "\"  -  " __DATE__,
                      COL_FOOT, small_scale);
}

void splash_draw(void) {
    if (theme_is_classic()) {
        /* Nel tema classico la schermata di avvio e' la console stessa: si
         * prepara e si scrive il banner come testo, cosi' scorre via. */
        fbcon_init();
        fbcon_classic_banner();
        return;
    }

    splash_compose(display_buffer(), display_width(), display_height());
}
