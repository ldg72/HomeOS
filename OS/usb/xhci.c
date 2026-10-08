/*
 * HomeOS — xHCI minimo per la Milk-V Mars.
 *
 * Solo quello che serve a una tastiera boot HID: bootstrap in polling, comandi
 * su command ring, trasferimenti di controllo su EP0 e interrupt IN su EP1.
 * Niente interrupt, niente stream, niente hub.
 *
 * La coerenza DMA si basa sul front-port coerente della JH7110: bastano le
 * barriere, come verificato dal bring-up di riferimento.
 */

#include <stddef.h>

#include "../console.h"
#include "../mmio.h"
#include "../services.h"
#include "usb.h"
#include "usb_hw.h"
#include "xhci.h"

#define XHCI_BASE_ADDR 0x10110000UL
#define PAGE 4096U

#define DMA_PAGES 13U          /* oltre agli scratchpad */

static uint64_t dmacast(const void *p) { return (uint64_t)(uintptr_t)p; }

static uint32_t read32(uintptr_t a) { uint32_t v = mmio_read32(a); mmio_fence(); return v; }
static void write32(uintptr_t a, uint32_t v) { mmio_write32(a, v); }

static void write64(uintptr_t a, uint64_t v) {
    write32(a, (uint32_t)v);
    write32(a + 4, (uint32_t)(v >> 32));
}

static void doorbell(struct xhci *x, uint32_t target, uint32_t value) {
    write32(x->db + target * 4U, value);
}

static int wait_sts(struct xhci *x, uint32_t mask, uint32_t want,
                    uint32_t ms, const char *label) {
    for (uint32_t i = 0; i < ms; i++) {
        if ((read32(x->op + XHCI_OP_USBSTS) & mask) == want) return 1;
        services_delay_ms(1);
    }
    os_puts("[usb] xHCI: timeout on ");
    os_puts(label);
    os_puts("\n");
    return 0;
}

static void ring_setup(struct xhci_trb *ring) {
    for (uint32_t i = 0; i < XHCI_RING_TRBS; i++) {
        ring[i].param = 0;
        ring[i].status = 0;
        ring[i].control = 0;
    }
}

/* Scrive il link TRB di chiusura e applica il toggle del ciclo. */
static void ring_wrap(struct xhci_trb *ring, uint32_t *enq, uint32_t *cycle) {
    struct xhci_trb *link = &ring[XHCI_RING_TRBS - 1U];
    link->param = dmacast(ring);
    link->status = 0;
    link->control = TRB_TYPE(TRB_LINK) | TRB_TOGGLE | *cycle;
    *enq = 0;
    *cycle ^= 1U;
}

/*
 * Command TRB: parametro nei dword 0-1, dword 2 = status, e lo SLOT ID va nei
 * bit 24-31 del dword 3 (il control), non nel dword 2. Metterlo nel posto
 * sbagliato fa leggere al controller slot 0, che e' riservato: TRB Error (11).
 */
static void cmd_post(struct xhci *x, uint32_t type, uint64_t param,
                     uint32_t status, uint32_t slot_id) {
    if (x->cmd_enq == XHCI_RING_TRBS - 1U) ring_wrap(x->cmd_ring, &x->cmd_enq, &x->cmd_cycle);
    struct xhci_trb *t = &x->cmd_ring[x->cmd_enq];
    t->param = param;
    t->status = status;
    t->control = TRB_TYPE(type) | ((slot_id & 0xffU) << 24) | x->cmd_cycle;
    mmio_fence();
    x->cmd_enq++;
    doorbell(x, 0, 0);
}

/* Estrae un evento dall'event ring; 0 se non ce ne sono. */
static int evt_next(struct xhci *x, struct xhci_trb *out) {
    struct xhci_trb *t = &x->evt_ring[x->evt_deq];
    if ((t->control & TRB_CYCLE) != x->evt_cycle) return 0;
    *out = *t;
    x->evt_deq++;
    if (x->evt_deq >= XHCI_RING_TRBS) {
        x->evt_deq = 0;
        x->evt_cycle ^= 1U;
    }
    mmio_fence();
    uint64_t erdp = dmacast(&x->evt_ring[x->evt_deq]) | XHCI_ERDP_EHB;
    write64(x->rt + XHCI_RT_ERDP, erdp);
    /* Si azzera anche il bit di interrupt pendente: si interroga a polling,
     * e lasciarlo alzato puo' far smettere il controller di segnalare. */
    uint32_t iman = read32(x->rt + XHCI_RT_IMAN);
    if (iman & 1U) write32(x->rt + XHCI_RT_IMAN, iman | 1U);
    return 1;
}

static uint32_t trb_type(const struct xhci_trb *t) {
    return (t->control >> 10) & 0x3fU;
}

static uint32_t trb_slot(const struct xhci_trb *t) { return (t->control >> 24) & 0xffU; }
static uint32_t trb_code(const struct xhci_trb *t) { return (t->status >> 24) & 0xffU; }

int xhci_init(struct xhci *x) {
    x->base = XHCI_BASE_ADDR;
    const uint32_t hcsparams1 = read32(x->base + 0x04);
    const uint32_t hcsparams2 = read32(x->base + 0x08);
    const uint32_t dboff = read32(x->base + 0x14);
    const uint32_t rtsoff = read32(x->base + 0x18);
    const uint32_t caplen = read32(x->base + 0x00) & 0xffU;

    x->op = x->base + caplen;
    x->rt = x->base + (rtsoff & ~0x1fU);
    x->db = x->base + (dboff & ~0x3U);
    x->max_slots = hcsparams1 & 0xffU;
    x->max_ports = (hcsparams1 >> 24) & 0xffU;

    uint32_t scratchpads = ((hcsparams2 >> 27) & 0x1fU) |
                           (((hcsparams2 >> 21) & 0x1fU) << 5);
    if (scratchpads > 31U) scratchpads = 31U;

    os_puts("[usb] xHCI: slots=");
    os_putu32(x->max_slots);
    os_puts(" porte=");
    os_putu32(x->max_ports);
    os_puts(" scratchpad=");
    os_putu32(scratchpads);
    os_puts("\n");

    x->dma_size = (DMA_PAGES + scratchpads) * PAGE;
    uint8_t *dma = (uint8_t *)services_alloc_dma(x->dma_size);
    if (!dma) {
        os_puts("[usb] xHCI: DMA allocation failed\n");
        return 0;
    }
    x->dma = dma;

    uint8_t *p = dma;
    x->dcbaa = (uint64_t *)p;              p += PAGE;
    uint64_t *scratch_array = (uint64_t *)p; p += PAGE;
    for (uint32_t i = 0; i < scratchpads; i++) {
        scratch_array[i] = dmacast(p + i * PAGE);
    }
    p += scratchpads * PAGE;
    x->cmd_ring = (struct xhci_trb *)p;    p += PAGE;
    x->evt_ring = (struct xhci_trb *)p;    p += PAGE;
    x->erst = (uint64_t *)p;               p += PAGE;
    x->input_ctx = (uint32_t *)p;          p += PAGE;
    x->device_ctx = (uint32_t *)p;         p += PAGE;
    x->ep0_ring = (struct xhci_trb *)p;    p += PAGE;
    x->ep1_ring = (struct xhci_trb *)p;    p += PAGE;
    x->data_buffer = p;                    p += PAGE;
    x->report_page = p;                    p += PAGE;

    for (uint32_t i = 0; i < PAGE / 8U; i++) x->dcbaa[i] = 0;
    if (scratchpads) x->dcbaa[0] = dmacast(scratch_array);
    ring_setup(x->cmd_ring);
    ring_setup(x->evt_ring);
    ring_setup(x->ep0_ring);
    ring_setup(x->ep1_ring);

    x->cmd_enq = 0;
    x->cmd_cycle = 1;
    x->evt_deq = 0;
    x->evt_cycle = 1;
    x->ep0_cycle = 1;
    x->ep0_enq = 0;
    x->ep1_cycle = 1;
    x->ep1_enq = 0;

    /* Ferma e resetta il controller. */
    write32(x->op + XHCI_OP_USBCMD, 0);
    if (!wait_sts(x, XHCI_STS_HALTED, XHCI_STS_HALTED, 100, "halt")) return 0;
    write32(x->op + XHCI_OP_USBCMD, XHCI_CMD_RESET);
    if (!wait_sts(x, XHCI_CMD_RESET, 0, 500, "reset")) return 0;
    if (!wait_sts(x, XHCI_STS_CNR, 0, 500, "controller not ready")) return 0;

    write32(x->op + XHCI_OP_CONFIG, x->max_slots);
    write64(x->op + XHCI_OP_DCBAAP, dmacast(x->dcbaa));

    /* Event ring + ERST, interrupter 0. */
    x->erst[0] = dmacast(x->evt_ring);
    x->erst[1] = XHCI_RING_TRBS;
    write32(x->rt + XHCI_RT_ERSTSZ, 1);
    write64(x->rt + XHCI_RT_ERSTBA, dmacast(x->erst));
    write64(x->rt + XHCI_RT_ERDP, dmacast(x->evt_ring) | XHCI_ERDP_EHB);
    write32(x->rt + XHCI_RT_IMAN, read32(x->rt + XHCI_RT_IMAN) | 1U); /* clear IP */

    write64(x->op + XHCI_OP_CRCR, dmacast(x->cmd_ring) | 1U);

    mmio_fence();
    write32(x->op + XHCI_OP_USBCMD, XHCI_CMD_RUN);
    if (!wait_sts(x, XHCI_STS_HALTED, 0, 100, "run")) return 0;
    return 1;
}

/* Attende un Command Completion e ne restituisce il TRB. */
static int wait_command(struct xhci *x, struct xhci_trb *out, uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        struct xhci_trb evt;
        while (evt_next(x, &evt)) {
            if (trb_type(&evt) == TRB_CMD_COMPLETE) {
                *out = evt;
                uint32_t code = trb_code(&evt);
                if (code != COMPLETION_SUCCESS) {
                    os_puts("[usb] xHCI: command failed, code=");
                    os_putu32(code);
                    os_puts("\n");
                    return 0;
                }
                return 1;
            }
        }
        services_delay_ms(1);
    }
    os_puts("[usb] xHCI: command timeout\n");
    return 0;
}

int xhci_enable_slot(struct xhci *x, uint32_t *slot_id_out) {
    struct xhci_trb evt;
    cmd_post(x, TRB_ENABLE_SLOT, 0, 0, 0);
    if (!wait_command(x, &evt, 500)) {
        os_puts("[usb] xHCI: Enable Slot failed\n");
        return 0;
    }
    *slot_id_out = trb_slot(&evt);
    return 1;
}

/* EP0 transfer ring: post del Setup, eventuale Data, Status. */
static void ep0_post(struct xhci *x, uint64_t param, uint32_t status, uint32_t control) {
    if (x->ep0_enq == XHCI_RING_TRBS - 1U) {
        struct xhci_trb *link = &x->ep0_ring[XHCI_RING_TRBS - 1U];
        link->param = dmacast(x->ep0_ring);
        link->status = 0;
        link->control = TRB_TYPE(TRB_LINK) | TRB_TOGGLE | x->ep0_cycle;
        x->ep0_enq = 0;
        x->ep0_cycle ^= 1U;
    }
    struct xhci_trb *t = &x->ep0_ring[x->ep0_enq];
    t->param = param;
    t->status = status;
    t->control = control | x->ep0_cycle;
    mmio_fence();
    x->ep0_enq++;
}

int xhci_address_device(struct xhci *x, uint32_t slot_id, uint32_t port, uint32_t speed) {
    uint32_t *icc = x->input_ctx;          /* Input Control Context */
    uint32_t *slot = x->input_ctx + 8;     /* Slot Context          */
    uint32_t *ep0 = x->input_ctx + 16;     /* EP0 Context           */
    uint32_t *out = x->device_ctx;
    struct xhci_trb evt;
    /* Nel campo "Port Speed" di PORTSC: 1 = full, 2 = low, 3 = high. */
    const uint32_t control_mps = (speed == 3U) ? 64U : 8U;

    /* I contesti endpoint sono 32 byte ciascuno: va azzerata tutta la pagina,
     * altrimenti i campi non scritti restano sporchi dal comando precedente. */
    for (uint32_t i = 0; i < PAGE / 4U; i++) x->input_ctx[i] = 0;
    for (uint32_t i = 0; i < PAGE / 4U; i++) x->device_ctx[i] = 0;
    ring_setup(x->ep0_ring);
    x->ep0_enq = 0;
    x->ep0_cycle = 1;

    icc[1] = 3U;                            /* add: slot + EP0 */

    slot[0] = (speed << 20) | (1U << 27);   /* route string 0, speed, 1 contesto EP */
    slot[1] = (port << 16);                 /* root hub port number */
    slot[2] = 0;
    slot[3] = 0;                            /* indirizzo 0 finche' non assegnato */

    /*
     * EP0: CErr=3 (obbligatorio per gli endpoint di controllo), tipo Control
     * Bidirectional, e la dimensione massima del pacchetto di controllo.
     *
     * Quest'ultima dipende dalla velocita': 8 byte per low e full speed, 64 per
     * high speed. La tastiera era low speed, quindi il valore fisso a 8 non
     * aveva mai dato problemi — ma un mouse high speed con 8 byte non si
     * indirizza, perche' il dispositivo si aspetta pacchetti da 64.
     * Senza CErr il controller risponde TRB Error.
     */
    ep0[1] = (3U << 1) | (4U << 3) | (control_mps << 16);
    /* TR Dequeue Pointer: campo nei bit 4..63, bit 0 = DCS (deve valere 1). */
    uint64_t ring_addr = dmacast(x->ep0_ring);
    ep0[2] = (uint32_t)((ring_addr & ~0xfULL) | 1U);
    ep0[3] = (uint32_t)(ring_addr >> 32);
    ep0[4] = 8U;

    x->dcbaa[slot_id] = dmacast(out);
    mmio_fence();

    cmd_post(x, TRB_ADDRESS_DEV, dmacast(x->input_ctx), 0, slot_id);
    if (!wait_command(x, &evt, 500)) {
        os_puts("[usb] xHCI: Address Device failed; ICC=");
        os_puthex64(x->input_ctx[1]);
        os_puts(" slot0=");
        os_puthex64(x->input_ctx[8]);
        os_puts(" slot1=");
        os_puthex64(x->input_ctx[9]);
        os_puts(" ep0_1=");
        os_puthex64(x->input_ctx[17]);
        os_puts(" ep0_2=");
        os_puthex64(x->input_ctx[18]);
        os_puts("\n");
        return 0;
    }
    return 1;
}

static uint32_t ep0_dci(void) { return 1U; }

int xhci_control(struct xhci *x, const uint8_t setup[8], void *buffer,
                 uint32_t length, int dir_in) {
    uint64_t setup_param = 0;
    for (int i = 0; i < 8; i++) setup_param |= ((uint64_t)setup[i]) << (8 * i);

    ep0_post(x, setup_param, 8U,
             TRB_TYPE(TRB_SETUP) | TRB_IDT | (dir_in ? TRB_DIR_IN : 0));
    if (length) {
        ep0_post(x, dmacast(buffer), length,
                 TRB_TYPE(TRB_DATA) | (dir_in ? TRB_DIR_IN : 0));
    }
    /* Lo status va nella direzione opposta ai dati. */
    ep0_post(x, 0, 0, TRB_TYPE(TRB_STATUS) | TRB_IOC | (length && dir_in ? 0 : TRB_DIR_IN));
    mmio_fence();

    doorbell(x, x->slot_id, ep0_dci());
    if (!xhci_wait_transfer(x, 1000)) return 0;
    return 1;
}

int xhci_wait_transfer(struct xhci *x, uint32_t timeout_ms) {
    for (uint32_t i = 0; i < timeout_ms; i++) {
        struct xhci_trb evt;
        while (evt_next(x, &evt)) {
            uint32_t type = trb_type(&evt);
            if (type == TRB_XFER_EVENT) {
                return trb_code(&evt) == COMPLETION_SUCCESS;
            }
            /* Un cambio di stato porta e' normale: si consuma e si prosegue. */
        }
        services_delay_ms(1);
    }
    return 0;
}

/*
 * Da invocare dopo xhci_init e dopo che la porta e' stata resettata: il reset
 * del controller (HCRST) azzera lo stato delle porte, quindi la sequenza
 * corretta e' controller -> porta -> slot.
 */
int xhci_attach(struct xhci *x, uint32_t port) {
    /* Test dell'anello: un No-Op deve completare. */
    struct xhci_trb evt;
    cmd_post(x, TRB_NOOP_CMD, 0, 0, 0);
    if (!wait_command(x, &evt, 500)) {
        os_puts("[usb] xHCI: No-Op did not complete\n");
        return 0;
    }

    uint32_t slot = 0;
    if (!xhci_enable_slot(x, &slot)) return 0;
    os_puts("[usb] xHCI: slot assigned = ");
    os_putu32(slot);
    os_puts("\n");
    x->slot_id = slot;

    uint32_t portsc = usb_portsc(port);
    uint32_t speed = (portsc >> 10) & 0xfU;
    x->port_speed = speed;
    if (!xhci_address_device(x, slot, port, speed)) return 0;
    os_puts("[usb] xHCI: device address assigned\n");
    return 1;
}

int xhci_start(struct xhci *x, uint32_t port) {
    x->base = XHCI_BASE_ADDR;
    return xhci_init(x) && xhci_attach(x, port);
}

uint32_t xhci_event_type(const struct xhci_trb *t) { return trb_type(t); }
uint32_t xhci_event_code(const struct xhci_trb *t) { return trb_code(t); }

int xhci_poll_event(struct xhci *x, struct xhci_trb *out) {
    return evt_next(x, out);
}

uint8_t *xhci_data_buffer(struct xhci *x) { return x->data_buffer; }

int xhci_configure_endpoint(struct xhci *x, uint32_t dci, uint32_t ep_type,
                            uint32_t max_packet, uint32_t interval,
                            struct xhci_trb *ring) {
    uint32_t *icc = x->input_ctx;
    uint32_t *slot = x->input_ctx + 8;
    uint32_t *ep = x->input_ctx + 8 + dci * 8;
    struct xhci_trb evt;

    for (uint32_t i = 0; i < PAGE / 4U; i++) x->input_ctx[i] = 0;

    icc[1] = 1U | (1U << dci);              /* aggiungi slot + endpoint dci */
    slot[0] = (x->port_speed << 20) | (dci << 27);
    slot[1] = (1U << 16);

    uint64_t ring_addr = dmacast(ring);
    ep[0] = (interval << 16);
    ep[1] = (3U << 1) | (ep_type << 3) | (max_packet << 16);
    ep[2] = (uint32_t)((ring_addr & ~0xfULL) | 1U);
    ep[3] = (uint32_t)(ring_addr >> 32);
    ep[4] = 8U;
    mmio_fence();

    cmd_post(x, TRB_CONFIG_EP, dmacast(x->input_ctx), 0, x->slot_id);
    if (!wait_command(x, &evt, 500)) {
        os_puts("[usb] xHCI: Configure Endpoint failed\n");
        return 0;
    }
    return 1;
}

int xhci_submit_in(struct xhci *x, uint32_t dci, struct xhci_trb *ring,
                   uint32_t *enq, uint32_t *cycle, void *buffer, uint32_t length) {
    if (*enq == XHCI_RING_TRBS - 1U) ring_wrap(ring, enq, cycle);
    uint32_t index = *enq;
    struct xhci_trb *t = &ring[index];
    t->param = dmacast(buffer);
    t->status = length;
    t->control = TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_DIR_IN | *cycle;
    mmio_fence();
    (*enq)++;
    doorbell(x, x->slot_id, dci);
    return (int)index;
}

uint8_t *xhci_report_slot(struct xhci *x, uint32_t index) {
    return x->report_page + index * 8u;
}
