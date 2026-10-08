/*
 * HomeOS — tastiera USB HID boot su xHCI.
 *
 * Enumerazione minima: descrittori device e config, SET_CONFIGURATION,
 * protocollo boot, un interrupt IN su EP1 e decodifica del report da 8 byte.
 * Nessuna keymap internazionale: solo la disposizione US, che basta alla shell.
 */

#include <stddef.h>

#include "../console.h"
#include "../mmio.h"
#include "../services.h"
#include "hid.h"
#include "usb_hw.h"
#include "xhci.h"

/* Richieste standard e di classe. */
#define REQ_GET_DESCRIPTOR    0x06U
#define REQ_SET_CONFIGURATION 0x09U
#define HID_SET_IDLE          0x0AU
#define HID_SET_PROTOCOL      0x0BU

#define DT_DEVICE    0x01U
#define DT_CONFIG    0x02U
#define DT_INTERFACE 0x04U
#define DT_ENDPOINT  0x05U

#define HID_CLASS     0x03U
#define HID_SUBCLASS_BOOT 0x01U
#define HID_PROTO_KEYBOARD 0x01U

#define EP_TYPE_INTERRUPT_IN 7U
#define EP1_IN_DCI 3U          /* EP1 IN: 2 * 1 + 1 */

/*
 * Il campo Interval dell'endpoint context non e' il bInterval del descrittore:
 * e' l'esponente della potenza di due di microframe che da' il periodo, cioe'
 * 2^(Interval-1) * 125 us. Passare il bInterval grezzo di una tastiera (10)
 * significava chiedere un'interrogazione ogni 64 ms: con un dito lento si
 * vedeva lo stesso, con due mani le pressioni piu' rapide del campionamento
 * andavano perse. Il valore corretto per 10 e' 3, cioe' circa 1 ms.
 */
static uint32_t interval_field(uint32_t binterval) {
    if (binterval == 0) return 0;
    uint32_t value = 1, exponent = 0;
    while ((value << 1) <= binterval && exponent < 15) {
        value <<= 1;
        exponent++;
    }
    return exponent;
}

#define QUEUE_SIZE 64U
#define REPORT_SLOTS 32U      /* trasferimenti in volo contemporaneamente:
                               * oltre 300 ms di report a 10 ms ciascuno */

static uint8_t g_previous[8];
static int g_have_previous;
static int g_streaming;
static uint8_t g_trb_slot[XHCI_RING_TRBS];
static const struct xhci_trb *g_ring_base;

/* Contatori diagnostici. */
static uint32_t g_reports, g_chars, g_failed;

/*
 * Ripetizione dei tasti tenuti: dopo mezzo secondo, poi ogni 50 ms.
 *
 * Si ripetono TUTTI i tasti premuti, non solo l'ultimo. Serve al puntatore:
 * tenendo premute due frecce deve proseguire in diagonale, non piegare da una
 * parte sola dopo il primo passo.
 */
static uint8_t g_held[6];
static int g_held_count;
static int g_repeat_armed;
static uint64_t g_repeat_since;
static uint64_t g_repeat_last;
static int g_shift_latched;

/* Ultimi report ricevuti: servono a capire se un tasto non e' mai arrivato
 * o se e' stato scartato in decodifica. */
static uint8_t g_history[USB_KBD_HISTORY][8];
static uint32_t g_history_count;
static int g_ready;

static volatile uint8_t g_queue[QUEUE_SIZE];
static uint32_t g_q_head, g_q_tail;

static uint8_t g_device_desc[18];
static uint16_t g_vendor, g_product;
static uint8_t g_endpoint_max_packet = 8;
static uint8_t g_endpoint_interval = 10;

static void setup_packet(uint8_t out[8], uint8_t type, uint8_t request,
                         uint16_t value, uint16_t index, uint16_t length) {
    out[0] = type;
    out[1] = request;
    out[2] = (uint8_t)value;
    out[3] = (uint8_t)(value >> 8);
    out[4] = (uint8_t)index;
    out[5] = (uint8_t)(index >> 8);
    out[6] = (uint8_t)length;
    out[7] = (uint8_t)(length >> 8);
}

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int get_descriptor(struct xhci *x, uint8_t type, uint8_t index,
                          uint16_t length, void *buffer) {
    uint8_t setup[8];
    setup_packet(setup, 0x80U, REQ_GET_DESCRIPTOR,
                 (uint16_t)((type << 8) | index), 0, length);
    return xhci_control(x, setup, buffer, length, 1);
}

/* Percorre il blob dei descrittori e prende interfaccia boot e endpoint IN. */
static int parse_configuration(struct xhci *x, const uint8_t *blob, uint16_t total) {
    uint32_t offset = 0;
    int in_boot_interface = 0;
    int found_interface = 0, found_endpoint = 0;

    while (offset + 2U <= total) {
        uint8_t length = blob[offset];
        uint8_t type = blob[offset + 1U];
        if (length < 2U || offset + length > total) break;

        if (type == DT_INTERFACE && length >= 9U) {
            uint8_t cls = blob[offset + 5U];
            uint8_t sub = blob[offset + 6U];
            uint8_t proto = blob[offset + 7U];
            in_boot_interface = (cls == HID_CLASS && sub == HID_SUBCLASS_BOOT &&
                                 proto == HID_PROTO_KEYBOARD);
            if (in_boot_interface) found_interface = 1;
        } else if (type == DT_ENDPOINT && length >= 7U && in_boot_interface) {
            uint8_t address = blob[offset + 2U];
            uint8_t attributes = blob[offset + 3U];
            if ((address & 0x80U) && (attributes & 0x03U) == 0x03U) {
                g_endpoint_max_packet = (uint8_t)rd16(&blob[offset + 4U]);
                g_endpoint_interval = blob[offset + 6U];
                found_endpoint = 1;
            }
        }
        offset += length;
    }

    if (!found_interface) {
        os_puts("[usb] HID: boot keyboard interface not found\n");
        return 0;
    }
    if (!found_endpoint) {
        os_puts("[usb] HID: interrupt IN endpoint not found\n");
        return 0;
    }
    (void)x;
    return 1;
}

/* --- keymap US -------------------------------------------------------- */

static const char keymap[128] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd', [0x08] = 'e',
    [0x09] = 'f', [0x0A] = 'g', [0x0B] = 'h', [0x0C] = 'i', [0x0D] = 'j',
    [0x0E] = 'k', [0x0F] = 'l', [0x10] = 'm', [0x11] = 'n', [0x12] = 'o',
    [0x13] = 'p', [0x14] = 'q', [0x15] = 'r', [0x16] = 's', [0x17] = 't',
    [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x', [0x1C] = 'y',
    [0x1D] = 'z',
    [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4', [0x22] = '5',
    [0x23] = '6', [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x28] = '\n', [0x29] = 27, [0x2A] = '\b', [0x2B] = '\t', [0x2C] = ' ',
    [0x2D] = '-', [0x2E] = '=', [0x2F] = '[', [0x30] = ']', [0x31] = '\\',
    [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',', [0x37] = '.',
    [0x38] = '/',
};

static const char keymap_shift[128] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D', [0x08] = 'E',
    [0x09] = 'F', [0x0A] = 'G', [0x0B] = 'H', [0x0C] = 'I', [0x0D] = 'J',
    [0x0E] = 'K', [0x0F] = 'L', [0x10] = 'M', [0x11] = 'N', [0x12] = 'O',
    [0x13] = 'P', [0x14] = 'Q', [0x15] = 'R', [0x16] = 'S', [0x17] = 'T',
    [0x18] = 'U', [0x19] = 'V', [0x1A] = 'W', [0x1B] = 'X', [0x1C] = 'Y',
    [0x1D] = 'Z',
    [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$', [0x22] = '%',
    [0x23] = '^', [0x24] = '&', [0x25] = '*', [0x26] = '(', [0x27] = ')',
    [0x28] = '\n', [0x29] = 27, [0x2A] = '\b', [0x2B] = '\t', [0x2C] = ' ',
    [0x2D] = '_', [0x2E] = '+', [0x2F] = '{', [0x30] = '}', [0x31] = '|',
    [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<', [0x37] = '>',
    [0x38] = '?',
};

static void queue_push(uint8_t c) {
    uint32_t next = (g_q_head + 1U) % QUEUE_SIZE;
    if (next == g_q_tail) return;   /* pieno: si scarta */
    g_queue[g_q_head] = c;
    g_q_head = next;
}

static int queue_pop(void) {
    if (g_q_tail == g_q_head) return -1;
    uint8_t c = g_queue[g_q_tail];
    g_q_tail = (g_q_tail + 1U) % QUEUE_SIZE;
    return c;
}

static int key_was_pressed(uint8_t code) {
    for (int i = 2; i < 8; i++) {
        if (g_previous[i] == code) return 1;
    }
    return 0;
}

static void emit_code(uint8_t code) {
    if (code == 0 || code >= 128U) return;
    char c;

    /* Le frecce non hanno un carattere: si passano come codici a parte,
     * indipendenti dal maiuscolo, e li consuma chi legge l'input. */
    switch (code) {
        case 0x52: c = (char)USB_KEY_UP;    break;
        case 0x51: c = (char)USB_KEY_DOWN;  break;
        case 0x50: c = (char)USB_KEY_LEFT;  break;
        case 0x4F: c = (char)USB_KEY_RIGHT; break;
        default:
            c = (g_shift_latched ? keymap_shift : keymap)[code];
            break;
    }

    if (!c) return;
    queue_push((uint8_t)c);
    g_chars++;
}

/*
 * I tasti nuovi generano subito il carattere; un tasto che resta premuto
 * diventa il candidato alla ripetizione, che avanza poi nel tempo fuori da
 * qui: questa funzione vede solo i cambiamenti di stato.
 */
static void decode_report(const uint8_t *report) {
    for (int i = 0; i < 8; i++) {
        g_history[g_history_count % USB_KBD_HISTORY][i] = report[i];
    }
    g_history_count++;

    g_shift_latched = (report[0] & 0x22U) != 0;   /* LShift | RShift */
    g_held_count = 0;

    for (int i = 2; i < 8; i++) {
        uint8_t code = report[i];
        if (code == 0) continue;
        if (g_held_count < 6) g_held[g_held_count++] = code;
        if (g_have_previous && key_was_pressed(code)) continue;
        emit_code(code);
        g_repeat_since = services_ticks();
        g_repeat_last = g_repeat_since;
        g_repeat_armed = 1;
    }
    if (g_held_count == 0) g_repeat_armed = 0;
    g_reports++;
}

/* Avanza la ripetizione se i tasti sono tenuti da abbastanza tempo. */
static void repeat_tick(void) {
    if (!g_repeat_armed || !g_held_count) return;
    uint64_t now = services_ticks();
    uint64_t delay = HOMEOS_TIMER_FREQ_HZ / 2u;    /* mezzo secondo */
    uint64_t period = HOMEOS_TIMER_FREQ_HZ / 20u;  /* poi ogni 50 ms */
    if (now - g_repeat_since < delay) return;
    if (now - g_repeat_last < period) return;
    for (int i = 0; i < g_held_count; i++) emit_code(g_held[i]);
    g_repeat_last = now;
}

void usb_kbd_stats(uint32_t *reports, uint32_t *chars, uint32_t *failed) {
    if (reports) *reports = g_reports;
    if (chars) *chars = g_chars;
    if (failed) *failed = g_failed;
}

uint32_t usb_kbd_history_count(void) { return g_history_count; }

const uint8_t *usb_kbd_history_at(uint32_t index) {
    if (index >= USB_KBD_HISTORY) return 0;
    if (g_history_count < USB_KBD_HISTORY) {
        return (index < g_history_count) ? g_history[index] : 0;
    }
    /* In ordine cronologico: il piu' vecchio sta dove e' ripartito l'anello. */
    uint32_t start = g_history_count % USB_KBD_HISTORY;
    return g_history[(start + index) % USB_KBD_HISTORY];
}

int usb_kbd_start(struct xhci *x) {
    uint8_t setup[8];
    uint8_t *buffer = xhci_data_buffer(x);

    os_puts("[usb] HID: reading descriptors\n");
    if (!get_descriptor(x, DT_DEVICE, 0, 18, g_device_desc)) {
        os_puts("[usb] HID: GET_DESCRIPTOR device failed\n");
        return 0;
    }
    g_vendor = rd16(&g_device_desc[8]);
    g_product = rd16(&g_device_desc[10]);
    os_puts("[usb] HID: device ");
    os_puthex64(g_vendor);
    os_puts(":");
    os_puthex64(g_product);
    os_puts("\n");

    if (!get_descriptor(x, DT_CONFIG, 0, 9, buffer)) {
        os_puts("[usb] HID: GET_DESCRIPTOR config (header) failed\n");
        return 0;
    }
    uint16_t total = rd16(&buffer[2]);
    if (total < 9U || total > 4096U) {
        os_puts("[usb] HID: invalid config length\n");
        return 0;
    }
    if (!get_descriptor(x, DT_CONFIG, 0, total, buffer)) {
        os_puts("[usb] HID: GET_DESCRIPTOR config failed\n");
        return 0;
    }
    if (!parse_configuration(x, buffer, total)) return 0;

    setup_packet(setup, 0x00U, REQ_SET_CONFIGURATION, 1, 0, 0);
    if (!xhci_control(x, setup, NULL, 0, 0)) {
        os_puts("[usb] HID: SET_CONFIGURATION failed\n");
        return 0;
    }

    /* Protocollo boot: report fisso da 8 byte, niente report descriptor. */
    setup_packet(setup, 0x21U, HID_SET_PROTOCOL, 0, 0, 0);
    if (!xhci_control(x, setup, NULL, 0, 0)) {
        os_puts("[usb] HID: SET_PROTOCOL(boot) failed\n");
        return 0;
    }
    setup_packet(setup, 0x21U, HID_SET_IDLE, 0, 0, 0);
    (void)xhci_control(x, setup, NULL, 0, 0);   /* non bloccante se fallisce */

    if (!xhci_configure_endpoint(x, EP1_IN_DCI, EP_TYPE_INTERRUPT_IN,
                                 g_endpoint_max_packet,
                                 interval_field(g_endpoint_interval),
                                 x->ep1_ring)) {
        return 0;
    }
    os_puts("[usb] HID: interrupt endpoint configured, MPS=");
    os_putu32(g_endpoint_max_packet);
    os_puts("\n");

    g_have_previous = 0;
    g_streaming = 0;
    g_q_head = g_q_tail = 0;
    g_reports = g_chars = g_failed = 0;
    g_history_count = 0;
    g_held_count = 0;
    g_repeat_armed = 0;
    g_ready = 1;
    return 1;
}

int usb_kbd_ready(void) { return g_ready; }

/* Accoda un trasferimento per lo slot indicato, ricordando dove e' finito. */
static void submit_slot(struct xhci *x, uint32_t slot) {
    int index = xhci_submit_in(x, EP1_IN_DCI, x->ep1_ring, &x->ep1_enq,
                               &x->ep1_cycle, xhci_report_slot(x, slot),
                               sizeof(g_previous));
    if (index >= 0) g_trb_slot[index] = (uint8_t)slot;
}

/* Dal completion event al TRB: il 'param' e' l'indirizzo del TRB completato. */
static uint32_t trb_index(const struct xhci_trb *event) {
    uintptr_t base = (uintptr_t)g_ring_base;
    uintptr_t addr = (uintptr_t)event->param;
    uintptr_t span = (uintptr_t)XHCI_RING_TRBS * sizeof(struct xhci_trb);
    if (addr < base || addr - base >= span) return XHCI_RING_TRBS;
    return (uint32_t)((addr - base) / sizeof(struct xhci_trb));
}

/*
 * Si tengono piu' trasferimenti in volo contemporaneamente. Con uno solo, fra
 * il completamento e la riaccodata successiva c'e' una finestra in cui la
 * tastiera puo' produrre un report che nessuno raccoglie: digitando veloce,
 * oppure mentre l'OS stampa molto testo, qualche tasto andava perso.
 */
int usb_kbd_try_getchar(struct xhci *x) {
    if (!g_ready) return -1;
    g_ring_base = x->ep1_ring;

    if (!g_streaming) {
        for (uint32_t i = 0; i < REPORT_SLOTS; i++) submit_slot(x, i);
        g_streaming = 1;
    }

    struct xhci_trb event;
    while (xhci_poll_event(x, &event)) {
        if (xhci_event_type(&event) != TRB_XFER_EVENT) continue;

        uint32_t trb = trb_index(&event);
        if (trb >= XHCI_RING_TRBS) continue;
        uint32_t slot = g_trb_slot[trb];
        uint8_t *report = xhci_report_slot(x, slot);

        if (xhci_event_code(&event) == COMPLETION_SUCCESS) {
            decode_report(report);
            for (int i = 0; i < 8; i++) g_previous[i] = report[i];
            g_have_previous = 1;
        } else {
            g_failed++;
        }
        submit_slot(x, slot);
    }
    repeat_tick();
    return queue_pop();
}
