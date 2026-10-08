#include "theme.h"
#include "version.h"

#define THEME_MODERN 0

/*
 * Ogni tema e' una riga di dati: colori, prompt e stile di presentazione.
 * Aggiungere una macchina significa aggiungere una voce alla tabella in fondo.
 *
 * Le tinte delle macchine d'epoca sono quelle delle loro tavolozze reali; per
 * il C128 si usa quella del VDC (sedici colori RGBI, la modalita' a 80
 * colonne), che e' l'unica cosa che distingue quella macchina dal C64: il
 * VIC-IIe a 40 colonne ha la stessa tavolozza del VIC-II.
 */

/* HomeOS: scuro, accento ciano, barra di intestazione. */
static const struct theme_colors theme_modern = {
    .name = "modern",
    .bg = 0xFF0B0F14u,
    .accent = 0xFF37D2F0u,
    .text = 0xFFD5DFE8u,
    .muted = 0xFF6B7A8Cu,
    .header_bg = 0xFF111820u,
    .header_text = 0xFF37D2F0u,
    .header_title = "HomeOS " HOMEOS_VERSION,
    .header_stripes = 0,
    .cursor = 0xFF37D2F0u,
    .prompt = "HomeOS> ",
    .classic = 0,
    .block_cursor = 0,
};

/*
 * La macchina a 8 bit: fondo blu, cornice e testo chiari, cursore a blocco.
 * Tutto dello stesso colore — testo, prompt, cursore, cornice — perche' e' la
 * tavolozza dell'epoca: non c'era un colore di richiamo.
 *
 * NON TOCCARE. Questa e' la tavolozza originale del VIC-II ed e' l'unico
 * riferimento vero alla macchina a 8 bit dentro HomeOS: colori, cornice a un
 * ventiquattresimo dell'altezza e prompt "READY." sono verificati a schermo e
 * vanno lasciati come sono. Per cambiare aspetto si aggiunge un tema, non si
 * modifica questo.
 */
static const struct theme_colors theme_c64 = {
    .name = "c64",
    .bg = 0xFF352879u,
    .accent = 0xFF6C5EB5u,
    .text = 0xFF6C5EB5u,
    .muted = 0xFF6C5EB5u,
    .border = 0xFF6C5EB5u,
    .border_div = 24u,
    .cursor = 0xFF6C5EB5u,
    .prompt = "READY.",
    .classic = 1,
    .block_cursor = 1,
};

/*
 * C128 in 80 colonne, dal vero: due soli colori. Il VDC genera il verde chiaro
 * e il grigio scuro dell'RGBI, e li usa per tutto — testo, cornice, cursore.
 *
 * Struttura identica al tema C64 — stessa cornice sottile, stessa intestazione
 * a righe di testo, stesso cursore a blocco, stesso "READY." — perche' quello
 * che distingue il C128 e' la larghezza dello schermo (80 colonne invece di
 * 40) e il VDC, non il disegno della cornice. Cambiano solo i colori.
 *
 * I valori sono campionati da una fotografia dello schermo (Screenshot
 * 2026-10-08), quindi portano dentro anche la resa del monitor: il verde
 * "elettrico" del VDC puro sarebbe #55FF55.
 */
static const struct theme_colors theme_c128 = {
    .name = "c128",
    .bg = 0xFF555555u,
    .accent = 0xFFACEA88u,
    .text = 0xFFACEA88u,
    .muted = 0xFFACEA88u,
    .border = 0xFFACEA88u,
    .border_div = 24u,
    .cursor = 0xFFACEA88u,
    .prompt = "READY.",
    .classic = 1,
    .block_cursor = 1,
};

/*
 * AmigaOS 1.3, dalla finestra della shell: fondo blu del Workbench, testo
 * bianco, barra del titolo bianca con scritta nera, cursore a blocco
 * arancione. I valori sono campionati da una fotografia dello schermo
 * (Screenshot 2026-10-08), non scelti a occhio.
 *
 * La barra del titolo porta due righe orizzontali del colore di sfondo, una
 * sopra e una sotto la scritta: e' il disegno delle finestre del Workbench, e
 * si vede chiaramente nella fotografia campionando riga per riga.
 * Attorno allo schermo corre una cornice bianca sottile, come il bordo della
 * finestra: un duecentoquarantesimo dell'altezza, cioe' tre pixel a 720.
 *
 * Il prompt vero si presentava come "1.HostFS:Hamurabi>": numero della
 * finestra, percorso corrente, poi ">". Qui non c'e' ancora un percorso
 * corrente da mostrare, quindi resta "1>".
 */
static const struct theme_colors theme_amiga = {
    .name = "amiga",
    .bg = 0xFF0055AAu,
    .accent = 0xFFFFFFFFu,
    .text = 0xFFFFFFFFu,
    .muted = 0xFF000000u,
    .header_bg = 0xFFFFFFFFu,
    .header_text = 0xFF000000u,
    .header_title = "HomeOS Shell",
    .header_h = 28u,
    .header_console_font = 1,
    .header_stripes = 1,
    .border = 0xFFFFFFFFu,
    .border_div = 240u,
    .cursor = 0xFFFF8800u,
    .prompt = "1>",
    .classic = 0,
    .block_cursor = 1,
};

static const struct theme_colors *const theme_table[] = {
    &theme_modern,
    &theme_c64,
    &theme_c128,
    &theme_amiga,
};

#define THEME_COUNT ((int)(sizeof(theme_table) / sizeof(theme_table[0])))

/* Tema di avvio: si sceglie a compilazione (make THEME=2 per il C64, 3 per il
 * C128, 4 per l'Amiga). */
#ifndef HOMEOS_DEFAULT_THEME
#define HOMEOS_DEFAULT_THEME 1
#endif

static int current =
    (HOMEOS_DEFAULT_THEME > 0 && HOMEOS_DEFAULT_THEME <= THEME_COUNT)
        ? (HOMEOS_DEFAULT_THEME - 1) : THEME_MODERN;

int theme_count(void) { return THEME_COUNT; }

const struct theme_colors *theme_at(int index) {
    if (index < 0 || index >= THEME_COUNT) return 0;
    return theme_table[index];
}

const struct theme_colors *theme_current(void) { return theme_table[current]; }

int theme_is_classic(void) { return theme_table[current]->classic; }

int theme_set(int index) {
    if (index < 0 || index >= THEME_COUNT) return 0;
    current = index;
    return 1;
}
