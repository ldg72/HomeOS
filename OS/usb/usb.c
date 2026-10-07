/*
 * HomeOS — tastiera USB: orchestrazione.
 *
 * Catena completa: parte elettrica -> xHCI -> enumerazione -> HID.
 * Ogni fase stampa un esito, cosi' un singolo boot dice dove si e' fermata.
 */

#include "../console.h"
#include "hid.h"
#include "usb.h"
#include "usb_hw.h"
#include "xhci.h"

static struct xhci g_xhci;
static int g_ready;

int usb_keyboard_start(void) {
    os_puts("[usb] starting USB keyboard\n");

    if (!usb_hw_bringup()) return 0;
    os_puts("[usb] electrical stage: ok\n");

    /* Ordine obbligatorio: prima il controller, poi la porta. */
    if (!xhci_init(&g_xhci)) return 0;

    if (!usb_hw_port_up()) return 0;

    if (!xhci_attach(&g_xhci, usb_active_port())) return 0;
    os_puts("[usb] controller and slot: ok\n");

    if (!usb_kbd_start(&g_xhci)) {
        os_puts("[usb] enumeration not completed\n");
        return 0;
    }
    if (!usb_kbd_ready()) return 0;

    g_ready = 1;
    os_puts("[usb] keyboard ready\n");
    return 1;
}

int usb_keyboard_ready(void) { return g_ready; }

int usb_keyboard_getchar(void) {
    if (!g_ready) return -1;
    return usb_kbd_try_getchar(&g_xhci);
}

void usb_keyboard_status(void) {
    os_puts("[usb] port 1 PORTSC = ");
    os_puthex64(usb_portsc(1));
    os_puts("\n[usb] keyboard ready: ");
    os_puts(g_ready ? "yes\n" : "no\n");
}

void usb_keyboard_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed) {
    usb_kbd_stats(reports, chars, failed);
}

uint32_t usb_keyboard_history_count(void) { return usb_kbd_history_count(); }

const uint8_t *usb_keyboard_history_at(uint32_t index) {
    return usb_kbd_history_at(index);
}
