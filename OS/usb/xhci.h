#ifndef HOMEOS_XHCI_H
#define HOMEOS_XHCI_H

#include <stdint.h>

/* Registri operational, relativi a xHCI + CAPLENGTH. */
#define XHCI_OP_USBCMD   0x00U
#define XHCI_OP_USBSTS   0x04U
#define XHCI_OP_PAGESIZE 0x08U
#define XHCI_OP_CRCR     0x18U
#define XHCI_OP_DCBAAP   0x30U
#define XHCI_OP_CONFIG   0x38U

#define XHCI_CMD_RUN   (1U << 0)
#define XHCI_CMD_RESET (1U << 1)

#define XHCI_STS_HALTED (1U << 0)
#define XHCI_STS_CNR    (1U << 11)
#define XHCI_STS_HSE    (1U << 2)
#define XHCI_STS_HCE    (1U << 12)

/* Interrupter 0, relativo a xHCI + RTSOFF. */
#define XHCI_RT_IMAN    0x20U
#define XHCI_RT_ERSTSZ  0x28U
#define XHCI_RT_ERSTBA  0x30U
#define XHCI_RT_ERDP    0x38U

#define XHCI_ERDP_EHB (1ULL << 3)

/* Tipi di TRB. */
#define TRB_NORMAL      1U
#define TRB_SETUP       2U
#define TRB_DATA        3U
#define TRB_STATUS      4U
#define TRB_LINK        6U
#define TRB_ENABLE_SLOT 9U
#define TRB_ADDRESS_DEV 11U
#define TRB_CONFIG_EP   12U
#define TRB_NOOP_CMD    23U
#define TRB_XFER_EVENT  32U
#define TRB_CMD_COMPLETE 33U
#define TRB_PORT_STATUS 34U

#define TRB_TYPE(n)     ((uint32_t)(n) << 10)
#define TRB_CYCLE       1U
#define TRB_TOGGLE      (1U << 1)
#define TRB_IOC         (1U << 5)
#define TRB_IDT         (1U << 6)
#define TRB_CHAIN       (1U << 4)
#define TRB_DIR_IN      (1U << 16)

/*
 * Codici di completamento di un transfer event.
 *
 * "Short packet" arriva quando il dispositivo manda meno byte di quanti ne
 * erano stati richiesti — ed e' un esito **buono**: i byte che sono arrivati
 * sono validi. Trattarlo come errore significava buttare via ogni report di
 * un mouse, che ne manda quattro su un endpoint che ne accetta sette.
 */
#define COMPLETION_SUCCESS      1U
#define COMPLETION_SHORT_PACKET 13U

#define XHCI_RING_TRBS 256U

struct xhci_trb {
    uint64_t param;
    uint32_t status;
    uint32_t control;
};

/* Stato del controller e degli anelli. */
struct xhci {
    uintptr_t base;        /* USB_XHCI_BASE            */
    uintptr_t op;          /* base + CAPLENGTH         */
    uintptr_t rt;          /* base + RTSOFF            */
    uintptr_t db;          /* base + DBOFF             */
    uint32_t  max_slots;
    uint32_t  max_ports;
    uint32_t  port_speed;

    uint8_t  *dma;
    uint32_t  dma_size;
    uint64_t *dcbaa;
    struct xhci_trb *cmd_ring;
    struct xhci_trb *evt_ring;
    uint64_t *erst;
    uint32_t *input_ctx;
    uint32_t *device_ctx;
    struct xhci_trb *ep0_ring;
    struct xhci_trb *ep1_ring;
    uint8_t  *data_buffer;
    uint8_t  *report_page;   /* buffer dei report HID, 8 byte ciascuno */

    uint32_t cmd_enq;
    uint32_t cmd_cycle;
    uint32_t evt_deq;
    uint32_t evt_cycle;

    uint32_t slot_id;
    uint32_t ep0_cycle;
    uint32_t ep0_enq;
    uint32_t ep1_cycle;
    uint32_t ep1_enq;
};

int  xhci_init(struct xhci *x);
/* Da chiamare DOPO xhci_init: il reset del controller azzera lo stato porte. */
int  xhci_attach(struct xhci *x, uint32_t port);
int  xhci_enable_slot(struct xhci *x, uint32_t *slot_id_out);
int  xhci_address_device(struct xhci *x, uint32_t slot_id, uint32_t port, uint32_t speed);

/* Trasferimento di controllo su EP0. */
int  xhci_control(struct xhci *x, const uint8_t setup[8], void *buffer,
                  uint32_t length, int dir_in);

/* Aggiunge un endpoint al device context e lo abilita. */
int  xhci_configure_endpoint(struct xhci *x, uint32_t dci, uint32_t ep_type,
                             uint32_t max_packet, uint32_t interval,
                             struct xhci_trb *ring);

/* Accoda un trasferimento interrupt IN sull'endpoint indicato. */
/* Accoda un trasferimento interrupt IN. Ritorna l'indice del TRB usato,
 * oppure -1: serve a risalire dal completion event al buffer riempito. */
int  xhci_submit_in(struct xhci *x, uint32_t dci, struct xhci_trb *ring,
                    uint32_t *enq, uint32_t *cycle, void *buffer, uint32_t length);

/* Buffer del report numero 'index' (8 byte ciascuno). */
uint8_t *xhci_report_slot(struct xhci *x, uint32_t index);

/* Preleva un evento, senza attendere: 0 se la coda e' vuota. */
int  xhci_poll_event(struct xhci *x, struct xhci_trb *out);

uint32_t xhci_event_type(const struct xhci_trb *t);
uint32_t xhci_event_code(const struct xhci_trb *t);

/* Attende e restituisce un Transfer Event del tipo richiesto. */
int  xhci_wait_transfer(struct xhci *x, uint32_t timeout_ms);

uint8_t *xhci_data_buffer(struct xhci *x);

#endif
