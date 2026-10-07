/*
 * HomeOS — schermata di avvio.
 *
 * Composizione volutamente essenziale: sfondo scuro sfumato, un marchio
 * geometrico (badge arrotondato con la H), il nome, e in piccolo versione,
 * architettura e scheda. Tutto disegnato con primitive nostre: nessuna
 * risorsa esterna, nessun font di sistema.
 */

#include "splash.h"
#include "display.h"
#include "fbcon.h"
#include "font_topaz.h"
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

void splash_draw(void) {
    const struct theme_colors *theme = theme_current();
    if (theme->retro) {
        /* Nel tema classico la schermata di avvio e' la console stessa: si
         * prepara e si scrive il banner come testo, cosi' scorre via. */
        fbcon_init();
        fbcon_classic_banner();
        return;
    }

    const uint32_t w = display_width();
    const uint32_t h = display_height();
    gfx_init(display_buffer(), w, h);

    /*
     * La composizione e' definita su una tela di 480 righe e scala con la
     * risoluzione: a 1080p marchio e testi raddoppiano, e il blocco resta
     * centrato in verticale invece di restare piccolo in alto.
     */
    const uint32_t s = (h >= 1000u) ? 2u : 1u;
    const uint32_t content_h = 400u * s;
    const uint32_t top = (h > content_h) ? (h - content_h) / 2u : 0u;

    /* Sfondo: sfumatura verticale scura. */
    gfx_vgradient(0, 0, w, h, COL_BG_TOP, COL_BG_BOT);

    /* Marchio: badge arrotondato con sfumatura ciano -> blu. */
    const uint32_t bw = 120u * s;
    const uint32_t bx = (w - bw) / 2u;
    const uint32_t by = top + 34u * s;
    gfx_rounded_gradient(bx, by, bw, bw, 26u * s, COL_ACCENT, COL_ACCENT2);

    /* H in negativo dentro il badge. */
    gfx_fill_rect(bx + 26u * s, by + 30u * s, 16u * s, 60u * s, COL_WHITE);
    gfx_fill_rect(bx + 78u * s, by + 30u * s, 16u * s, 60u * s, COL_WHITE);
    gfx_fill_rect(bx + 26u * s, by + 52u * s, 68u * s, 16u * s, COL_WHITE);

    /* Nome, grande, con sfumatura bianco -> ciano. */
    gfx_text_centered_gradient(top + 188u * s, "HomeOS", COL_WHITE, COL_ACCENT, 4u * s);

    /* Riga di separazione. */
    gfx_fill_rect((w - 120u * s) / 2u, top + 272u * s, 120u * s, 3u * s, COL_ACCENT);

    /* Architettura e scheda, in piccolo. */
    gfx_text_centered(top + 292u * s, HOMEOS_ARCH "  -  RISC-V 64-bit", COL_TEXT, 2u * s);
    gfx_text_centered(top + 328u * s, HOMEOS_BOARD, COL_MUTED, 2u * s);

    /* Core sottostante. */
    gfx_text_centered(top + 364u * s,
                      "running on " HOMEOS_CORE_NAME " " HOMEOS_CORE_VERSION,
                      COL_MUTED, 1u * s);

    /* Piede: versione e data di build, minuto. */
    gfx_text_centered(top + 404u * s,
                      "HomeOS " HOMEOS_VERSION " \"" HOMEOS_CODENAME "\"  -  " __DATE__,
                      COL_FOOT, 1u * s);
}
