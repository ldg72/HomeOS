#ifndef HOMEOS_GPIO_H
#define HOMEOS_GPIO_H

/*
 * GPIO di sistema della JH7110 (blocco SYS IOMUX a 0x13040000).
 *
 * Modello dei registri, ricostruito dal driver Linux e verificato sui valori
 * che avevamo gia' usato per il VBUS USB:
 *
 *   DOEN  0x000 + (pin & ~3)   campo 6 bit a 8*(pin&3): 0 = uscita, 1 = ingresso
 *   DOUT  0x040 + (pin & ~3)   campo 7 bit: 0 = basso, 1 = alto,
 *                              valori maggiori selezionano sorgenti interne
 *   GPI   0x080 + (pin & ~3)   instrada l'ingresso verso una periferica
 *   DIN   0x118 (pin 0-31) / 0x11c (32-63), 1 bit per pin
 *   PAD   0x120 + pin*4 per i pin 0..74 (bit 0 = input enable)
 */

#include <stdint.h>

#define GPIO_OK        0
#define GPIO_ERR_PIN  (-1)

/* Numero massimo di GPIO gestiti dal blocco: 64. */
#define GPIO_MAX_PIN 63u

int gpio_set_output(uint32_t pin, int value);
int gpio_set_input(uint32_t pin);

/* Livello attuale del pin: 0 o 1, oppure GPIO_ERR_PIN. */
int gpio_read(uint32_t pin);

/* Configurazione corrente: 1 = uscita, 0 = ingresso, negativo = errore. */
int gpio_is_output(uint32_t pin);

/*
 * Verifica la catena senza strumenti esterni: mette il pin in uscita, lo porta
 * basso e lo rilegge, poi alto e lo rilegge. Su un pin libero il livello letto
 * deve seguire quello guidato.
 * Ritorna 1 se il test passa, 0 se fallisce, negativo su errore.
 */
int gpio_self_test(uint32_t pin);

#endif
