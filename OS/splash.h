#ifndef HOMEOS_SPLASH_H
#define HOMEOS_SPLASH_H

#include <stdint.h>

/* Schermata di avvio: logo, nome, versione e architettura. */
void splash_draw(void);

/*
 * Compone il marchio su un buffer XRGB8888 di dimensioni date, senza passare
 * dal display. La usa il boot e la usa l'anteprima sul computer di sviluppo
 * (tools/splash_preview.c): cosi' il disegno si puo' guardare e correggere
 * prima di scrivere la microSD.
 */
void splash_compose(volatile uint32_t *buffer, uint32_t width, uint32_t height);

#endif
