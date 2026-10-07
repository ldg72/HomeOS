#ifndef HOMEOS_THEME_H
#define HOMEOS_THEME_H

/*
 * Tema visivo: colori della console, bordo e stile della schermata di avvio.
 * E' un asse indipendente dalla risoluzione: si possono combinare liberamente.
 */

#include <stdint.h>

struct theme_colors {
    const char *name;
    uint32_t bg;         /* sfondo dell'area di testo              */
    uint32_t accent;     /* prompt e richiami                      */
    uint32_t text;       /* output normale                         */
    uint32_t muted;      /* testo secondario                       */
    uint32_t header_bg;  /* barra superiore; ignorato se c'e' bordo */
    uint32_t border;     /* colore del bordo; 0 = nessun bordo      */
    int retro;           /* 1 = schermata di avvio in stile classico */
};

int theme_count(void);
const struct theme_colors *theme_at(int index);
const struct theme_colors *theme_current(void);

/* 1 se il tema attivo e' quello classico. */
int theme_is_retro(void);

/* Imposta il tema. Non ridisegna: lo fa il chiamante. */
int theme_set(int index);

#endif
