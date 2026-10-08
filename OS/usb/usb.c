/*
 * HomeOS — input USB: orchestrazione.
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

static const char *kind_name(int kind) {
    if (kind == USB_INPUT_MOUSE) return "mouse";
    if (kind == USB_INPUT_KEYBOARD) return "keyboard";
    return "nothing";
}

int usb_input_start(void) {
    os_puts("[usb] starting USB input\n");

    if (!usb_hw_bringup()) return 0;
    os_puts("[usb] electrical stage: ok\n");

    /* Ordine obbligatorio: prima il controller, poi la porta. */
    if (!xhci_init(&g_xhci)) return 0;

    if (!usb_hw_port_up()) return 0;

    if (!xhci_attach(&g_xhci, usb_active_port())) return 0;
    os_puts("[usb] controller and slot: ok\n");

    if (!usb_hid_start(&g_xhci)) {
        os_puts("[usb] enumeration not completed\n");
        return 0;
    }
    if (!usb_hid_ready()) return 0;

    g_ready = 1;
    os_puts("[usb] ready: ");
    os_puts(kind_name(usb_hid_kind()));
    os_puts("\n");
    return 1;
}

int usb_input_ready(void) { return g_ready; }

int usb_input_kind(void) {
    if (!g_ready) return USB_INPUT_NONE;
    return usb_hid_kind();
}

const char *usb_input_kind_name(void) { return kind_name(usb_input_kind()); }

int usb_input_getchar(void) {
    if (!g_ready) return -1;
    return usb_hid_poll(&g_xhci);
}

uint8_t usb_input_mouse_buttons(void) { return usb_mouse_buttons(); }

void usb_input_status(void) {
    os_puts("[usb] port 1 PORTSC = ");
    os_puthex64(usb_portsc(1));
    os_puts("\n[usb] ready: ");
    os_puts(g_ready ? kind_name(usb_hid_kind()) : "no");
    os_puts("\n");
}

void usb_input_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed) {
    usb_hid_stats(reports, chars, failed);
}

uint32_t usb_input_history_count(void) { return usb_hid_history_count(); }

const uint8_t *usb_input_history_at(uint32_t index) {
    return usb_hid_history_at(index);
}
