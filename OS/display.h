#ifndef HOMEOS_DISPLAY_H
#define HOMEOS_DISPLAY_H

/*
 * HomeOS — display JH7110 / Milk-V Mars.
 *
 * Monolitico e Mars-only: nessun device, nessuna interfaccia, nessuna ABI
 * Exec64OS. La sequenza e' quella validata fisicamente sulla scheda, portata
 * dal backend display di Exec64 (fonte: docs/hardware/marsfb_v2.md).
 *
 * Regola di memoria: il controller legge dal fisico 0x70000000, la CPU scrive
 * solo attraverso l'alias non cachato 0x470000000.
 */

#include <stdint.h>

#define DISPLAY_OK            0
#define DISPLAY_ERR_BAD_STATE (-3)
#define DISPLAY_ERR_TIMEOUT   (-4)
#define DISPLAY_ERR_HARDWARE  (-5)
#define DISPLAY_ERR_MODE      (-7)

/* Fasi, in ordine. Ognuna e' richiamabile separatamente per diagnosi. */
int display_power_on(void);     /* dominio VOUT acceso                     */
int display_clocks_on(void);    /* clock, reset, routing, identita' DC     */
int display_phy_on(void);       /* pre/post PLL e TMDS                     */
int display_start(uint32_t width, uint32_t height); /* timing + scanout   */

/* Sequenza completa + riempimento a colore pieno. */
int display_bringup(uint32_t width, uint32_t height, uint32_t color);

/* Cambio di modalita': non e' un cambio "a caldo". Spegne l'uscita, rifa' lock
 * delle PLL e timing: il monitor perde e riaggancia il segnale (~100 ms) e il
 * contenuto del framebuffer va ridisegnato dal chiamante. */
int display_set_mode(uint32_t width, uint32_t height);

/* Modalita' disponibili. */
int display_mode_count(void);
int display_mode_at(int index, uint32_t *width, uint32_t *height);
const char *display_mode_name(int index);

/* Riempie tutto lo schermo col colore XRGB8888 (l'X viene ignorato). */
void display_fill(uint32_t color);

/* Carta di prova: barre verticali, gradiente e bordo.
 * Serve a validare geometria, stride e ordine dei canali: un riempimento
 * uniforme non li distingue, questa immagine si'. */
void display_test_pattern(void);

volatile uint32_t *display_buffer(void);
uint32_t display_width(void);
uint32_t display_height(void);

/* Ultimo errore: registro, valore atteso, valore letto. */
uintptr_t display_failed_reg(void);
uint32_t  display_expected(void);
uint32_t  display_observed(void);

#endif
