#ifndef HOMEOS_USB_HID_H
#define HOMEOS_USB_HID_H

#include "xhci.h"
#include "usb.h"

/*
 * Il dispositivo trovato all'avvio. Con un ricevitore wireless, che e' tastiera
 * e mouse insieme, si sceglie la tastiera: il mouse si prende quando e' da solo.
 */
#define USB_HID_KIND_NONE     0
#define USB_HID_KIND_KEYBOARD 1
#define USB_HID_KIND_MOUSE    2

int usb_hid_start(struct xhci *x);
int usb_hid_ready(void);
int usb_hid_kind(void);

/* Restituisce il prossimo carattere digitato, oppure -1. Il mouse non produce
 * caratteri: quando arriva un suo report, questa funzione muove il puntatore e
 * restituisce -1. */
int usb_hid_poll(struct xhci *x);

/* Stato dei pulsanti del mouse (bit 0 = sinistro), per il clic. */
uint8_t usb_mouse_buttons(void);

/*
 * Tasti che non producono un carattere. Il valore sta fuori dall'ASCII, cosi'
 * chi legge l'input li distingue da quello che si digita: oggi sono le quattro
 * frecce, che servono a muovere il puntatore prima che esista il mouse.
 */
#define USB_KEY_UP    0x81
#define USB_KEY_DOWN  0x82
#define USB_KEY_LEFT  0x83
#define USB_KEY_RIGHT 0x84

/* Contatori: report ricevuti, caratteri emessi, trasferimenti falliti.
 * Servono a capire dove si perdono i tasti. */
void usb_hid_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed);

/* Cronologia degli ultimi report ricevuti, per diagnosi. */
uint32_t usb_hid_history_count(void);
const uint8_t *usb_hid_history_at(uint32_t index);   /* 8 byte */

#endif
