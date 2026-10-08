#ifndef HOMEOS_USB_H
#define HOMEOS_USB_H

/*
 * Input USB HID su Milk-V Mars (xHCI JH7110): tastiera o mouse.
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

/* Che cosa e' stato trovato sulla porta. */
#define USB_INPUT_NONE     0
#define USB_INPUT_KEYBOARD 1
#define USB_INPUT_MOUSE    2

/* Esegue l'intera catena: elettrica -> xHCI -> enumerazione -> HID.
 * Ritorna 0 se il dispositivo e' pronto e utilizzabile. */
int usb_input_start(void);

/* 1 se il dispositivo e' configurato e il polling e' attivo. */
int usb_input_ready(void);

/* USB_INPUT_KEYBOARD, USB_INPUT_MOUSE oppure USB_INPUT_NONE. */
int usb_input_kind(void);

/* Nome del dispositivo trovato, per i messaggi: "keyboard", "mouse",
 * "none". */
const char *usb_input_kind_name(void);

/*
 * Restituisce il prossimo carattere disponibile, oppure -1.
 * Non bloccante: va richiamata dal loop della shell.
 * Il mouse non produce caratteri: muove il puntatore e restituisce -1.
 */
int usb_input_getchar(void);

/* Pulsanti del mouse (bit 0 = sinistro). */
uint8_t usb_input_mouse_buttons(void);

/* Diagnostica: stato sintetico su console. */
void usb_input_status(void);

/* Contatori del driver: report ricevuti, caratteri emessi, errori. */
void usb_input_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed);

/* Cronologia degli ultimi report HID. */
#define USB_KBD_HISTORY 16u
uint32_t usb_input_history_count(void);
const uint8_t *usb_input_history_at(uint32_t index);

#endif
