/*
 * HomeOS — bring-up elettrico dell'host USB della Milk-V Mars.
 *
 * Trascrizione della sequenza validata fisicamente sulla scheda. Ordine e
 * valori non sono negoziabili: ognuno corrisponde a un test reale.
 *
 *   1. STG clock/reset + mode host
 *   2. VBUS: pinmux di SYS GPIO25 come dal DTS Milk-V Mars
 *   3. PHY USB2 con i bit del driver Linux phy-jh7110-usb
 *   4. routing over-current su costante alta
 *   5. alimentazione porta e reset porta
 */

#include "../console.h"
#include "../mmio.h"
#include "../services.h"
#include "usb.h"
#include "usb_hw.h"

/* --- blocchi MMIO ------------------------------------------------------ */
#define USB_OTG_BASE     0x10100000UL
#define USB_XHCI_BASE    0x10110000UL
#define USB_DEV_BASE     0x10120000UL
#define USB_PHY_BASE     0x10200000UL
#define STG_CRG_BASE     0x10230000UL
#define STG_SYSCON_BASE  0x10240000UL
#define SYS_CRG_BASE     0x13020000UL
#define SYS_SYSCON_BASE  0x13030000UL
#define SYS_GPIO_BASE    0x13040000UL

#define STG_USB_MODE_OFFSET     0x04U
#define STG_RESET_ASSERT_OFFSET 0x74U
#define STG_RESET_STATUS_OFFSET 0x78U

#define CLK_ENABLE   0x80000000U

/* Modalita' host: strap, suspend, bypass, PLL, refclk. */
#define USB_STRAP_MASK    (7U << 16)
#define USB_STRAP_HOST    (1U << 17)
#define USB_SUSPENDM_MASK (1U << 19)
#define USB_SUSPENDM_HOST (1U << 19)
#define USB_MISC_CFG_MASK (0xfU << 20)
#define USB_SUSPENDM_BYPS (1U << 20)
#define USB_PLL_EN        (1U << 22)
#define USB_REFCLK_MODE   (1U << 23)

#define USB_RESET_MASK   ((1U << 7) | (1U << 8) | (1U << 9) | (1U << 10))

/* VBUS: SYS GPIO25, pin nel campo alto di ogni registro. */
#define VBUS_PIN        25U
#define VBUS_SHIFT      8U
#define VBUS_DOEN_ADDR  (SYS_GPIO_BASE + 0x018UL)
#define VBUS_DOUT_ADDR  (SYS_GPIO_BASE + 0x058UL)
#define VBUS_PADCFG_ADDR (SYS_GPIO_BASE + 0x184UL)
#define VBUS_FUNC_ADDR  (SYS_GPIO_BASE + 0x2a0UL)
#define VBUS_DOEN_MASK  (0x3fU << VBUS_SHIFT)
#define VBUS_DOUT_MASK  (0x7fU << VBUS_SHIFT)
#define VBUS_FUNC_MASK  (0x3U << 15)
#define VBUS_PADCFG_MASK ((1U << 6) | (1U << 5) | (1U << 4) | (1U << 3) | (1U << 0))
#define GPOEN_ENABLE    0U
#define GPOUT_DRIVE_VBUS 7U   /* GPOUT_SYS_USB_DRIVE_VBUS */

/* Over-current: selettore della sorgente GPI 2. */
#define GPI_ROUTE_ADDR  (SYS_GPIO_BASE + 0x080UL)
#define GPI_USB_OVERCURRENT 2U
#define GPI_ROUTE_SHIFT (8U * (GPI_USB_OVERCURRENT % 4U))
#define GPI_ROUTE_MASK  (0x7fU << GPI_ROUTE_SHIFT)
#define GPI_SELECT_CONSTANT_HIGH 1U

/* PHY USB2 (phy-jh7110-usb). */
#define SYSCLK_USB_125M_ADDR   (SYS_CRG_BASE + (95U * 4U))
#define USB_PHY_CLK_MODE_ADDR  (USB_PHY_BASE + 0x00UL)
#define USB_PHY_LS_KEEPALIVE_ADDR (USB_PHY_BASE + 0x04UL)
#define SYS_USB_SPLIT_ADDR     (SYS_SYSCON_BASE + 0x18UL)
#define USB_CLK_MODE_RX_NORMAL_PWR (1U << 1)
#define USB_LS_KEEPALIVE_ENABLE    (1U << 4)
#define USB_PDRSTN_SPLIT           (1U << 17)

/* Porte xHCI: operational base = xHCI + CAPLENGTH (0x80). */
#define XHCI_OP_BASE     (USB_XHCI_BASE + 0x80UL)
#define XHCI_PORT_BASE   (XHCI_OP_BASE + 0x400UL)
#define XHCI_PORT_STRIDE 0x10UL

#define PORT_CONNECT   (1U << 0)
#define PORT_ENABLED   (1U << 1)
#define PORT_OVERCURRENT (1U << 3)
#define PORT_RESET     (1U << 4)
#define PORT_PLS_MASK  (0xfU << 5)
#define PORT_POWER     (1U << 9)
#define PORT_SPEED_MASK (0xfU << 10)

#define PORT_RO_MASK  (PORT_CONNECT | PORT_OVERCURRENT | PORT_SPEED_MASK)
#define PORT_RWS_MASK (PORT_PLS_MASK | PORT_POWER)

static void update(uintptr_t address, uint32_t mask, uint32_t value) {
    mmio_write32(address, (mmio_read32(address) & ~mask) | (value & mask));
}

static uint32_t g_active_port;

uint32_t usb_active_port(void) { return g_active_port; }

/* Stampa lo stato di una porta in forma leggibile: quando qualcosa non torna,
 * questo dice se il problema e' connessione, alimentazione o link. */
void usb_port_dump(const char *label, uint32_t port) {
    uint32_t sc = usb_portsc(port);
    os_puts(label);
    os_puts(" = ");
    os_puthex64(sc);
    os_puts("  connect=");
    os_puts((sc & PORT_CONNECT) ? "yes" : "no");
    os_puts(" enabled=");
    os_puts((sc & PORT_ENABLED) ? "yes" : "no");
    os_puts(" power=");
    os_puts((sc & PORT_POWER) ? "yes" : "no");
    os_puts(" oca=");
    os_puts((sc & PORT_OVERCURRENT) ? "yes" : "no");
    os_puts(" pls=");
    os_putu32((sc >> 5) & 0xfU);
    os_puts(" speed=");
    os_putu32((sc >> 10) & 0xfU);
    os_puts("\n");
}

static int wait_bit(uintptr_t address, uint32_t mask, uint32_t want, uint32_t ms) {
    uint32_t elapsed = 0;
    while (elapsed < ms) {
        if ((mmio_read32(address) & mask) == want) return 1;
        services_delay_ms(1);
        elapsed++;
    }
    return 0;
}

static int stg_power_on(void) {
    os_puts("[usb] STG: host mode, clock and reset\n");
    update(STG_SYSCON_BASE + STG_USB_MODE_OFFSET,
           USB_STRAP_MASK | USB_SUSPENDM_MASK | USB_MISC_CFG_MASK,
           USB_STRAP_HOST | USB_SUSPENDM_HOST | USB_SUSPENDM_BYPS |
           USB_PLL_EN | USB_REFCLK_MODE);

    /* usb0_apb=1, utmi_apb=2, axi=3, lpm=4, stb=5, app_125=6 */
    const uint32_t ids[6] = { 4U, 5U, 1U, 3U, 2U, 6U };
    for (int i = 0; i < 6; i++) {
        update(STG_CRG_BASE + ids[i] * 4U, CLK_ENABLE, CLK_ENABLE);
    }

    update(STG_CRG_BASE + STG_RESET_ASSERT_OFFSET, USB_RESET_MASK, 0);
    if (!wait_bit(STG_CRG_BASE + STG_RESET_STATUS_OFFSET, USB_RESET_MASK,
                  USB_RESET_MASK, 50)) {
        os_puts("[usb] STG: resets not released\n");
        return 0;
    }
    return 1;
}

static void vbus_on(void) {
    os_puts("[usb] VBUS: SYS GPIO25 pinmux\n");
    update(VBUS_DOUT_ADDR, VBUS_DOUT_MASK, GPOUT_DRIVE_VBUS << VBUS_SHIFT);
    update(VBUS_DOEN_ADDR, VBUS_DOEN_MASK, GPOEN_ENABLE << VBUS_SHIFT);
    update(VBUS_FUNC_ADDR, VBUS_FUNC_MASK, 0);
    update(VBUS_PADCFG_ADDR, VBUS_PADCFG_MASK, 0);
}

static void phy_on(void) {
    os_puts("[usb] PHY USB2\n");
    update(SYSCLK_USB_125M_ADDR, 0, CLK_ENABLE);
    update(USB_PHY_CLK_MODE_ADDR, 0, USB_CLK_MODE_RX_NORMAL_PWR);
    update(USB_PHY_LS_KEEPALIVE_ADDR, 0, USB_LS_KEEPALIVE_ENABLE);
    update(SYS_USB_SPLIT_ADDR, 0, USB_PDRSTN_SPLIT);
}

/* Il falso over-current nasce dal selettore lasciato sulla costante bassa:
 * con la costante alta OCA torna inattivo senza alcuna periferica collegata. */
static void overcurrent_high(void) {
    os_puts("[usb] over-current routing -> constant high\n");
    update(GPI_ROUTE_ADDR, GPI_ROUTE_MASK, GPI_SELECT_CONSTANT_HIGH << GPI_ROUTE_SHIFT);
}

uint32_t usb_portsc(uint32_t port) {
    return mmio_read32(XHCI_PORT_BASE + (port - 1U) * XHCI_PORT_STRIDE);
}

static void port_power_on(uint32_t port) {
    uintptr_t addr = XHCI_PORT_BASE + (port - 1U) * XHCI_PORT_STRIDE;
    uint32_t neutral = mmio_read32(addr) & (PORT_RO_MASK | PORT_RWS_MASK);
    mmio_write32(addr, neutral | PORT_POWER);
}

int usb_port_reset(uint32_t port) {
    uintptr_t addr = XHCI_PORT_BASE + (port - 1U) * XHCI_PORT_STRIDE;
    uint32_t neutral = mmio_read32(addr) & (PORT_RO_MASK | PORT_RWS_MASK);

    mmio_write32(addr, neutral | PORT_RESET);
    if (!wait_bit(addr, PORT_RESET, 0, 250)) {
        os_puts("[usb] reset: PORT_RESET bit did not clear\n");
        return 0;
    }
    if (!wait_bit(addr, PORT_ENABLED, PORT_ENABLED, 250)) {
        os_puts("[usb] reset: port did not reach enabled\n");
        return 0;
    }
    return 1;
}

int usb_hw_bringup(void) {
    if (!stg_power_on()) return 0;
    vbus_on();
    phy_on();
    overcurrent_high();
    services_delay_ms(30);

    /* Si alimentano entrambe le porte: costa nulla e non obbliga a indovinare
     * dove sia collegata la tastiera. */
    os_puts("[usb] powering ports\n");
    port_power_on(1);
    port_power_on(2);
    services_delay_ms(50);
    usb_port_dump("[usb] PORT1SC", 1);
    usb_port_dump("[usb] PORT2SC", 2);

    uint32_t port = 0;
    if (usb_portsc(1) & PORT_CONNECT) port = 1;
    else if (usb_portsc(2) & PORT_CONNECT) port = 2;

    if (port == 0) {
        os_puts("[usb] no device connected: check the port\n");
        return 0;
    }

    os_puts("[usb] device on port ");
    os_putu32(port);
    g_active_port = port;
    return 1;
}

/*
 * Il reset della porta va fatto dopo il reset del controller: HCRST riporta le
 * porte allo stato disconnesso, quindi un reset eseguito prima viene cancellato
 * e Address Device fallisce con la porta non abilitata.
 */
int usb_hw_port_up(void) {
    uint32_t port = g_active_port;
    if (port == 0) return 0;

    os_puts("[usb] port ");
    os_putu32(port);
    os_puts(": power and reset after the controller\n");
    port_power_on(port);
    services_delay_ms(20);

    int ok = 0;
    for (int attempt = 0; attempt < 3 && !ok; attempt++) {
        os_puts("[usb] port reset, attempt ");
        os_putu32((uint32_t)(attempt + 1));
        os_puts("\n");
        ok = usb_port_reset(port);
        if (!ok) services_delay_ms(50);
    }
    if (!ok) {
        usb_port_dump("[usb] PORTSC after attempts", port);
        return 0;
    }
    usb_port_dump("[usb] PORTSC after reset", port);
    return 1;
}
