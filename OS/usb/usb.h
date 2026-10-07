#ifndef HOMEOS_USB_H
#define HOMEOS_USB_H

/*
 * Tastiera USB HID su Milk-V Mars (xHCI JH7110).
 *
 * Tutti i valori di registro vengono dal bring-up gia' validato fisicamente
 * sulla scheda: accensione STG, VBUS, PHY USB2, routing over-current, porta,
 * bootstrap xHCI e round-trip DMA.
 *
 * HomeOS gira in S-Mode: accesso MMIO diretto, nessuna whitelist e nessuna
 * syscall. La memoria DMA si alloca con AllocMem e il puntatore e' gia'
 * un indirizzo fisico sotto 4 GiB.
 */

#include <stdint.h>

/* Esegue l'intera catena: elettrica -> xHCI -> enumerazione -> HID.
 * Ritorna 0 se la tastiera e' pronta e utilizzabile. */
int usb_keyboard_start(void);

/* 1 se la tastiera e' configurata e il polling e' attivo. */
int usb_keyboard_ready(void);

/* Restituisce il prossimo carattere disponibile, oppure -1.
 * Non bloccante: va richiamata dal loop della shell. */
int usb_keyboard_getchar(void);

/* Diagnostica: stato sintetico su console. */
void usb_keyboard_status(void);

/* Contatori del driver: report ricevuti, caratteri emessi, errori. */
void usb_keyboard_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed);

/* Cronologia degli ultimi report HID. */
#define USB_KBD_HISTORY 16u
uint32_t usb_keyboard_history_count(void);
const uint8_t *usb_keyboard_history_at(uint32_t index);

#endif
