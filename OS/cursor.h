#ifndef HOMEOS_CURSOR_H
#define HOMEOS_CURSOR_H

#include <stdint.h>

#include "cursor_image.h"

/*
 * Cursore hardware del DC8200.
 *
 * Il display controller ha un piano cursore tutto suo, sopra il piano primario
 * su cui disegna la console: un'immagine ARGB8888 di 32x32 o 64x64 pixel, che
 * si sposta scrivendo la sua posizione in un registro. Muovere il puntatore
 * non costa un pixel di disegno — che e' esattamente quello che serve qui,
 * perche' il framebuffer lo scriviamo attraverso l'alias non cachato.
 *
 * L'immagine deve stare in memoria che il controller legge senza passare
 * dalla cache: l'unica zona con l'alias non cachato e' la riserva
 * framebuffer. Il cursore si mette subito dopo lo schermo visibile, quindi a
 * 1080p — dove la riserva e' esattamente piena — non c'e' posto.
 */

#define CURSOR_OK             0
#define CURSOR_ERR_NO_DISPLAY (-1)   /* display non inizializzato      */
#define CURSOR_ERR_NO_ROOM    (-2)   /* riserva framebuffer piena      */

/* Prepara l'immagine e accende il cursore, in alto a sinistra. */
int cursor_init(void);

/* Sposta il puntatore; le coordinate vengono tenute dentro lo schermo. */
void cursor_move(int32_t x, int32_t y);

void cursor_enable(int on);
int  cursor_is_enabled(void);
int  cursor_is_ready(void);

/*
 * Codice del campo "size" del registro di configurazione. Il driver di
 * riferimento lo programma senza spiegare la codifica dei tre bit: si prova.
 * 0 e 1 sono i due valori sensati (32x32 e 64x64).
 */
void     cursor_set_size_code(uint32_t code);
uint32_t cursor_size_code(void);

/* Forma del puntatore: vedi enum cursor_shape in cursor_image.h. */
void cursor_set_shape(int shape);
int  cursor_shape(void);

/* Posizione corrente, e descrizione testuale dello stato per la shell. */
void cursor_position(uint16_t *x, uint16_t *y);

/* Indirizzo fisico dell'immagine e sua collocazione, per diagnostica. */
uint32_t cursor_image_phys(void);
uint32_t cursor_image_offset(void);

/*
 * Lettura dei due registri che possono spiegare un cursore che non compare:
 * la configurazione (ha accettato i bit?) e il clock gating, che il driver di
 * riferimento non tocca ma che su questa scheda potrebbe servire.
 */
void cursor_diag(uint32_t *config, uint32_t *clk_gating);

#endif
