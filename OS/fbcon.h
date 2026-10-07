#ifndef HOMEOS_FBCON_H
#define HOMEOS_FBCON_H

/*
 * Console di testo sul framebuffer.
 *
 * Griglia di celle 8x16 nel font Topaz, con cursore, scroll e una barra di
 * intestazione. Il contenuto e' tenuto in un buffer di caratteri: cosi' dopo
 * una schermata a tutto schermo (logo, carta di prova) la console si puo'
 * ridisegnare senza aver perso nulla.
 */

void fbcon_init(void);   /* prepara console e intestazione */
void fbcon_redo(void);   /* ridisegna intestazione e contenuto */
void fbcon_clear(void);  /* svuota l'area di testo */

/* Schermata di avvio del tema classico: la sola intestazione, senza testo. */
void fbcon_classic_boot_screen(void);

/* Banner di avvio del tema classico, scritto come testo (non come intestazione). */
void fbcon_classic_banner(void);

/* Fa lampeggiare il cursore; va richiamata mentre si attende l'input. */
void fbcon_cursor_blink(void);

void fbcon_putc(char c);
void fbcon_puts(const char *text);
void fbcon_puts_accent(const char *text);

#endif
