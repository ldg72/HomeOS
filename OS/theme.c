#include "theme.h"

#define THEME_MODERN 0

/* Aspetto attuale: scuro, accento ciano, barra di intestazione. */
static const struct theme_colors theme_modern = {
    "modern", 0xFF0B0F14u, 0xFF37D2F0u, 0xFFD5DFE8u, 0xFF6B7A8Cu,
    0xFF111820u, 0x00000000u, 0
};

/*
 * Stile classico delle macchine a 8 bit: fondo blu, bordo chiaro, testo chiaro.
 * I colori sono quelli della tavolozza a 16 colori di quell'epoca, ma la
 * schermata di avvio parla della nostra macchina, non di un'altra.
 */
static const struct theme_colors theme_retro = {
    /* Tutto dello stesso colore — testo, prompt, cursore, cornice — sul fondo
     * blu. E' la tavolozza dell'epoca: non c'era un colore di richiamo. */
    "retro", 0xFF352879u, 0xFF6C5EB5u, 0xFF6C5EB5u, 0xFF6C5EB5u,
    0x00000000u, 0xFF6C5EB5u, 1
};

static const struct theme_colors *const theme_table[] = {
    &theme_modern,
    &theme_retro,
};

#define THEME_COUNT ((int)(sizeof(theme_table) / sizeof(theme_table[0])))

/* Tema di avvio: si sceglie a compilazione (make THEME=2 per il retro). */
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

int theme_is_retro(void) { return theme_table[current]->retro; }

int theme_set(int index) {
    if (index < 0 || index >= THEME_COUNT) return 0;
    current = index;
    return 1;
}
