#ifndef HOMEOS_THEME_H
#define HOMEOS_THEME_H

/*
 * Tema visivo: colori della console, bordo e stile della schermata di avvio.
 * E' un asse indipendente dalla risoluzione: si possono combinare liberamente.
 *
 * Un tema e' una riga di dati in theme.c. Aggiungere una macchina significa
 * aggiungere una voce li': il resto dell'OS non cambia.
 */

#include <stdint.h>

struct theme_colors {
    const char *name;
    uint32_t bg;         /* sfondo dell'area di testo              */
    uint32_t accent;     /* prompt e richiami                      */
    uint32_t text;       /* output normale                         */
    uint32_t muted;      /* testo secondario                       */
    uint32_t header_bg;  /* barra superiore (solo stile moderno)    */
    uint32_t header_text;/* testo della barra superiore             */
    const char *header_title; /* scritta della barra                */
    uint32_t header_h;   /* altezza della barra; 0 = quella di serie */
    int header_console_font; /* scritta col carattere della console */
    int header_stripes;  /* due righe del colore di sfondo fra le due scritte */
    uint32_t border;     /* colore della cornice                    */
    uint32_t border_div; /* spessore = altezza / questo valore; 0 = nessuna cornice */
    uint32_t cursor;     /* colore del cursore                      */
    const char *prompt;  /* prompt della shell                      */
    /*
     * Presentazione da macchina a 8 bit: niente barra fissa (banner e dati
     * sono righe di testo che scorrono via), cursore a blocco. E' indipendente
     * dal colore e dalla cornice: il C64 ha la cornice, il C128 in 80 colonne
     * e l'Amiga no.
     */
    int classic;
    /* Cursore pieno invece che a barretta: l'Amiga ce l'aveva a blocco anche
     * senza la presentazione classica. */
    int block_cursor;
};

int theme_count(void);
const struct theme_colors *theme_at(int index);
const struct theme_colors *theme_current(void);

/* 1 se il tema attivo usa la presentazione classica. */
int theme_is_classic(void);

/* Imposta il tema. Non ridisegna: lo fa il chiamante. */
int theme_set(int index);

#endif
