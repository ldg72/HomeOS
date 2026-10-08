#ifndef HOMEOS_USB_HID_H
#define HOMEOS_USB_HID_H

#include "xhci.h"
#include "usb.h"

int usb_kbd_start(struct xhci *x);
int usb_kbd_ready(void);
int usb_kbd_try_getchar(struct xhci *x);

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
void usb_kbd_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed);

/* Cronologia degli ultimi report ricevuti, per diagnosi. */
uint32_t usb_kbd_history_count(void);
const uint8_t *usb_kbd_history_at(uint32_t index);   /* 8 byte */

#endif
