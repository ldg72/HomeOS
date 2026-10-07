#ifndef HOMEOS_USB_HID_H
#define HOMEOS_USB_HID_H

#include "xhci.h"
#include "usb.h"

int usb_kbd_start(struct xhci *x);
int usb_kbd_ready(void);
int usb_kbd_try_getchar(struct xhci *x);

/* Contatori: report ricevuti, caratteri emessi, trasferimenti falliti.
 * Servono a capire dove si perdono i tasti. */
void usb_kbd_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed);

/* Cronologia degli ultimi report ricevuti, per diagnosi. */
uint32_t usb_kbd_history_count(void);
const uint8_t *usb_kbd_history_at(uint32_t index);   /* 8 byte */

#endif
