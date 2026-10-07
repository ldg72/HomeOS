#ifndef HOMEOS_USB_HW_H
#define HOMEOS_USB_HW_H

#include <stdint.h>

int usb_hw_bringup(void);
int usb_port_reset(uint32_t port);
uint32_t usb_portsc(uint32_t port);
uint32_t usb_active_port(void);
void usb_port_dump(const char *label, uint32_t port);
/* Alimenta e resetta la porta scelta. Va chiamata DOPO il reset del controller. */
int usb_hw_port_up(void);

#endif
