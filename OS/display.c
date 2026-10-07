/*
 * HomeOS — sequenza display JH7110.
 *
 * Portata dal backend validato sulla Milk-V Mars. I valori dei registri qui
 * sotto non sono invenzioni: sono quelli che hanno prodotto un segnale HDMI
 * corretto sulla scheda (640x480 XRGB8888, stride 2560).
 *
 * Deliberatamente assente: EDID, audio, InfoFrame, IRQ, vblank, cursor,
 * overlay, gamma. Un solo modo fisso, un solo primary plane lineare.
 */

#include <stdint.h>

#include "console.h"
#include "display.h"
#include "mmio.h"
#include "services.h"

/* Blocchi MMIO (TRM JH7110). */
#define PMU    0x17030000UL
#define SYS    0x13020000UL
#define DC     0x29400000UL
#define HDMI   0x29590000UL
#define ROUTE  0x295b0000UL   /* DOM VOUT SYSCON */
#define VOUT   0x295c0000UL   /* DOM VOUT CRG    */

#define BIT(n) (1u << (n))
#define GATE  BIT(31)
#define LOCAL_RESETS (BIT(0) | BIT(1) | BIT(2) | BIT(9))
#define HREG(n) (HDMI + 4u * (n))   /* registri byte su bus a 32 bit */

/* Framebuffer riservato dal Core: il DC usa l'indirizzo basso, la CPU l'alias. */
#define MARSFB_PHYS     0x70000000UL
#define MARSFB_UNCACHED 0x470000000UL

struct video_mode {
    const char *name;
    uint32_t width, height;
    uint32_t htotal, hsync_start, hsync_end;
    uint32_t vtotal, vsync_start, vsync_end;
    uint8_t positive_hsync, positive_vsync;
    uint8_t pre_a1, pre_a2, pre_a3, pre_a4, pre_a5, pre_a6;
    uint8_t frac_d1, frac_d2, frac_d3;
    uint8_t post_ab, post_ac, post_ad;
};

/* Pixel clock 25,175 MHz (59,94 Hz), timing VGA, sincronismi negativi. */
static const struct video_mode mode_vga = {
    "VGA", 640, 480, 800, 656, 752, 525, 490, 492, 0, 0,
    1, 0x40, 100, 0x2f, 0x6c, 0x64, 0xf5, 0x55, 0x55,
    1, 80, 13
};

/* 1920x1080p60: timing CEA, sincronismi positivi, pixel clock 148,5 MHz.
 * Verificata fisicamente sulla scheda nel progetto di origine. */
static const struct video_mode mode_1080p = {
    "1080p", 1920, 1080, 2200, 2008, 2052, 1125, 1084, 1089, 1, 1,
    1, 0x70, 99, 0x15, 0x41, 0x42, 0, 0, 0,
    1, 20, 1
};

/* 1280x720p60: timing CEA-861 (1650x750), sincronismi positivi, pixel clock
 * 74,25 MHz. Le tuple PLL sono derivate dalla tabella del driver Linux:
 *   pre  prediv=1 fbdiv=99 tmds_div 1/2/2 pclk_div 1/2/3/4 frac=0
 *   post prediv=1 fbdiv=20 postdiv=1
 * I due modi gia' verificati sulla scheda (640x480 e 1080p) corrispondono alla
 * stessa tabella byte per byte, quindi questa derivazione e' affidabile — ma
 * e' l'unico dei tre modi non ancora provato fisicamente in HomeOS. */
static const struct video_mode mode_720p = {
    "720p", 1280, 720, 1650, 1390, 1430, 750, 725, 730, 1, 1,
    1, 0x70, 99, 0x1A, 0x41, 0x64, 0, 0, 0,
    1, 20, 1
};

static const struct video_mode *const mode_table[] = {
    &mode_vga,
    &mode_720p,
    &mode_1080p,
};

#define MODE_COUNT ((int)(sizeof(mode_table) / sizeof(mode_table[0])))

static const struct video_mode *active_mode;
static uint32_t stage;
static uint32_t identity_model, identity_revision, identity_customer;
static uintptr_t failed_reg;
static uint32_t expected_value, observed_value;

static const struct video_mode *mode_for(uint32_t width, uint32_t height) {
    for (int i = 0; i < MODE_COUNT; i++) {
        if (mode_table[i]->width == width && mode_table[i]->height == height) {
            return mode_table[i];
        }
    }
    return 0;
}

int display_mode_count(void) { return MODE_COUNT; }

int display_mode_at(int index, uint32_t *width, uint32_t *height) {
    if (index < 0 || index >= MODE_COUNT) return 0;
    if (width) *width = mode_table[index]->width;
    if (height) *height = mode_table[index]->height;
    return 1;
}

const char *display_mode_name(int index) {
    if (index < 0 || index >= MODE_COUNT) return 0;
    return mode_table[index]->name;
}

static uint32_t read32(uintptr_t address) {
    uint32_t value = mmio_read32(address);
    mmio_fence();
    return value;
}

static void write32(uintptr_t address, uint32_t value) {
    mmio_write32(address, value);
}

static void update(uintptr_t address, uint32_t mask, uint32_t value) {
    write32(address, (read32(address) & ~mask) | (value & mask));
}

static int fault(int code, uintptr_t reg, uint32_t want, uint32_t got) {
    failed_reg = reg;
    expected_value = want;
    observed_value = got;
    return code;
}

static int poll(uintptr_t reg, uint32_t mask, uint32_t want, uint32_t us) {
    uint64_t start = services_ticks();
    uint64_t limit = ((uint64_t)HOMEOS_TIMER_FREQ_HZ * us) / 1000000u;
    uint32_t value;
    do {
        value = read32(reg);
        if ((value & mask) == want) return DISPLAY_OK;
    } while (services_ticks() - start < limit);
    return fault(DISPLAY_ERR_TIMEOUT, reg, want, value);
}

/* I bit di stato del JH7110 valgono 1 quando il reset e' rilasciato. */
static int reset(uintptr_t control, uintptr_t status, uint32_t mask, int assert) {
    update(control, mask, assert ? mask : 0);
    return poll(status, mask, assert ? 0 : mask, 1000);
}

static void hwrite(uint32_t reg, uint32_t value) {
    write32(HREG(reg), value & 255u);
}

static void hword(uint32_t reg, uint32_t value) {
    hwrite(reg, value);
    hwrite(reg + 1, value >> 8);
}

int display_power_on(void) {
    if (stage != 0) return DISPLAY_OK;

    if (!(read32(PMU + 0x80) & BIT(4))) {
        write32(PMU + 0x0c, BIT(4));
        write32(PMU + 0x44, 0xff);
        write32(PMU + 0x44, 0x05);
        write32(PMU + 0x44, 0x50);
        int rc = poll(PMU + 0x80, BIT(4), BIT(4), 10000);
        if (rc != DISPLAY_OK) return rc;
    }
    stage = 1;
    return DISPLAY_OK;
}

int display_clocks_on(void) {
    int rc;
    if (stage != 1) return DISPLAY_ERR_BAD_STATE;

    identity_model = identity_revision = identity_customer = 0;

    /* Si conserva il PLL2 del firmware e si divide solo il ramo display. */
    update(SYS + 0xec, 0xffffff, 5);  /* divider VOUT_AXI, non un gate */
    update(SYS + 0xe8, GATE, GATE);   /* VOUT_SRC                     */
    update(SYS + 0x28, GATE, GATE);   /* AHB1 (condiviso)             */
    update(SYS + 0xf0, GATE, GATE);   /* NOC_BUS_DISP_AXI             */
    update(SYS + 0xf4, GATE, GATE);   /* VOUT_TOP_AHB                 */
    update(SYS + 0xf8, GATE, GATE);   /* VOUT_TOP_AXI                 */
    update(SYS + 0xfc, GATE, GATE);   /* HDMI MCLK, audio resta off   */

    rc = reset(SYS + 0x2f8, SYS + 0x308, BIT(26), 0);
    if (rc != DISPLAY_OK) return rc;
    rc = reset(SYS + 0x2fc, SYS + 0x30c, BIT(11), 0);
    if (rc != DISPLAY_OK) return rc;

    update(VOUT + 0x00, 0xffffff, 4); /* divider APB                */
    update(VOUT + 0x04, 0xffffff, 4); /* divider pixel interno      */
    update(VOUT + 0x10, GATE, GATE);
    update(VOUT + 0x14, GATE, GATE);
    update(VOUT + 0x18, GATE, GATE);
    update(VOUT + 0x1c, GATE | 0x3f000000u, GATE);
    update(VOUT + 0x20, GATE | 0x3f000000u, GATE);
    update(VOUT + 0x3c, GATE, GATE);
    update(VOUT + 0x40, GATE, GATE);
    update(VOUT + 0x44, GATE, GATE);

    rc = reset(VOUT + 0x48, VOUT + 0x4c, LOCAL_RESETS, 1);
    if (rc != DISPLAY_OK) return rc;
    services_delay_us(10);
    rc = reset(VOUT + 0x48, VOUT + 0x4c, LOCAL_RESETS, 0);
    if (rc != DISPLAY_OK) return rc;

    /*
     * Identita' completa prima di decidere: il driver vendor StarFive
     * riconosce la revisione 0x5720 e non richiede il model register.
     * Sulla Mars sono stati osservati revision=0x5720, customer=0x30e.
     * model=0 e' ammesso solo con quella coppia esatta.
     */
    identity_model = read32(DC + 0x20);
    identity_revision = read32(DC + 0x24);
    identity_customer = read32(DC + 0x30);

    if (identity_model != 0 && identity_model != 0x8200)
        return fault(DISPLAY_ERR_HARDWARE, DC + 0x20, 0x8200, identity_model);
    if (identity_revision != 0x5720)
        return fault(DISPLAY_ERR_HARDWARE, DC + 0x24, 0x5720, identity_revision);
    if (identity_customer != 0x30e)
        return fault(DISPLAY_ERR_HARDWARE, DC + 0x30, 0x30e, identity_customer);

    write32(DC + 0x14, 0);      /* nessun IRQ del DC in questo driver */
    write32(DC + 0x1480, 0);

    /* Panel 0 / DPI / RGB888, preservando i bit SYSCON estranei. */
    update(ROUTE + 4, 0x7e000000, 0x0c000000);
    update(ROUTE + 8, BIT(4), 0);

    /* Release digital/analogico, poi il register-clock TMDS obbligatorio. */
    update(HREG(0), BIT(5), BIT(5));
    services_delay_us(150);
    update(HREG(0), BIT(6), BIT(6));
    services_delay_us(150);
    update(HREG(0), 0x1f, 0x11);

    hwrite(0x05, 3);   /* video e audio muti finche' non e' completo */
    hwrite(0xc0, 0);   /* EDID interrupt mascherati                 */
    hwrite(0xc8, 0);   /* hotplug IRQ mascherato, HPD resta leggibile */

    stage = 2;
    return DISPLAY_OK;
}

int display_phy_on(void) {
    int rc;
    if (stage != 2) return DISPLAY_ERR_BAD_STATE;
    if (!active_mode) return DISPLAY_ERR_MODE;

    update(HREG(0x1b0), BIT(2), BIT(2));
    hwrite(0x1cc, 0x0f);

    /* I registri della pre-PLL Innosilicon hanno un bias di +0x100. */
    update(HREG(0x1a0), BIT(0) | BIT(1), BIT(0));
    hwrite(0x1a1, active_mode->pre_a1);
    hwrite(0x1a2, active_mode->pre_a2);
    hwrite(0x1a3, active_mode->pre_a3);
    hwrite(0x1a4, active_mode->pre_a4);
    hwrite(0x1a5, active_mode->pre_a5);
    hwrite(0x1a6, active_mode->pre_a6);
    hwrite(0x1d3, active_mode->frac_d3);
    hwrite(0x1d2, active_mode->frac_d2);
    hwrite(0x1d1, active_mode->frac_d1);
    update(HREG(0x1a0), BIT(0), 0);

    rc = poll(HREG(0x1a9), BIT(0), BIT(0), 100000);
    if (rc != DISPLAY_OK) return rc;

    /* Mux del pixel clock dal DC verso il PHY. */
    update(VOUT + 0x1c, 0x3f000000, BIT(24));
    update(VOUT + 0x20, 0x3f000000, BIT(24));

    update(HREG(0x1aa), BIT(0), BIT(0));
    hwrite(0x1ab, active_mode->post_ab);
    hwrite(0x1ac, active_mode->post_ac);
    hwrite(0x1ad, active_mode->post_ad);
    hwrite(0x1aa, 0x0e);

    rc = poll(HREG(0x1af), BIT(0), BIT(0), 100000);
    if (rc != DISPLAY_OK) return rc;

    hwrite(0x1b4, 0x07);  /* LDO      */
    hwrite(0x1be, 0x71);  /* serializer */
    hwrite(0x1b2, 0x8f);  /* TMDS     */

    stage = 3;
    return DISPLAY_OK;
}

int display_start(uint32_t width, uint32_t height) {
    if (stage != 3) return DISPLAY_ERR_BAD_STATE;
    const struct video_mode *mode = mode_for(width, height);
    if (!mode) return DISPLAY_ERR_MODE;
    active_mode = mode;

    uint32_t hblank = active_mode->htotal - active_mode->width;
    uint32_t hdelay = active_mode->htotal - active_mode->hsync_start;
    uint32_t hduration = active_mode->hsync_end - active_mode->hsync_start;
    uint32_t vblank = active_mode->vtotal - active_mode->height;
    uint32_t vdelay = active_mode->vtotal - active_mode->vsync_start;
    uint32_t vduration = active_mode->vsync_end - active_mode->vsync_start;
    uint32_t polarity = 1u | (active_mode->positive_hsync ? BIT(3) : 0) |
                        (active_mode->positive_vsync ? BIT(2) : 0);

    /* TMDS DVI-compatible: HDCP off, RGB full range, nessun CSC. */
    hwrite(0x52, 0);
    hwrite(0x01, 1);  hwrite(0x02, 0x30);
    hwrite(0x03, 1);  hwrite(0x04, 0x18);

    hwrite(0x08, polarity);
    hword(0x09, active_mode->htotal);
    hword(0x0b, hblank);
    hword(0x0d, hdelay);
    hword(0x0f, hduration);
    hword(0x11, active_mode->vtotal);
    hwrite(0x13, vblank);
    hwrite(0x14, vdelay);
    hwrite(0x15, vduration);

    write32(DC + 0x1ccc, 0);
    write32(DC + 0x1430, active_mode->width | (active_mode->htotal << 16));
    write32(DC + 0x1438, active_mode->hsync_start |
            (active_mode->hsync_end << 15) | BIT(30) |
            (active_mode->positive_hsync ? 0 : BIT(31)));
    write32(DC + 0x1440, active_mode->height | (active_mode->vtotal << 16));
    write32(DC + 0x1448, active_mode->vsync_start |
            (active_mode->vsync_end << 15) | BIT(30) |
            (active_mode->positive_vsync ? 0 : BIT(31)));

    write32(DC + 0x1400, MARSFB_PHYS);            /* base fisica           */
    write32(DC + 0x1408, active_mode->width * 4); /* stride in byte        */
    write32(DC + 0x1518, 5u << 26);               /* lineare XRGB8888      */
    write32(DC + 0x1810, active_mode->width | (active_mode->height << 15));
    write32(DC + 0x24d8, 0);
    write32(DC + 0x24e0, active_mode->width | (active_mode->height << 15));
    write32(DC + 0x2510, BIT(1));                 /* opaco, senza blend    */
    write32(DC + 0x1cc0, BIT(13) | BIT(12));      /* primary0 enable+commit */
    write32(DC + 0x1cd0, 0);                      /* DPI, non DP           */
    write32(DC + 0x14b8, 5);                      /* RGB888                */
    write32(DC + 0x1418, 0x1111);                 /* DE/data/clock/run     */
    mmio_fence();

    write32(DC + 0x1ccc, 1);   /* panel 0                             */
    write32(DC + 0x2518, 1);   /* commit dei registri shadow          */
    hwrite(0x05, 2);           /* video visibile, audio ancora muto   */

    stage = 4;
    return DISPLAY_OK;
}

/*
 * Spegne l'uscita mantenendo acceso il dominio: video muto, plane e TMDS
 * fermati, mux del pixel clock riportati al clock interno. E' il preludio a
 * una riprogrammazione del timing.
 */
static void display_output_off(void) {
    hwrite(0x05, 3);                       /* video e audio muti            */
    write32(DC + 0x1ccc, 0);               /* scanout fermo                 */
    update(DC + 0x1418, BIT(12), 0);
    write32(DC + 0x2518, BIT(0));
    hwrite(0x1b2, 0);
    hwrite(0x1be, 0);
    hwrite(0x1b4, 0);
    update(HREG(0x1aa), BIT(0), BIT(0));
    /* I mux del DC tornano al clock interno prima di fermare il PHY. */
    update(VOUT + 0x1c, 0x3f000000, 0);
    update(VOUT + 0x20, 0x3f000000, 0);
    update(HREG(0x1a0), BIT(0), BIT(0));
    update(HREG(0x1b0), BIT(2), 0);
    hwrite(0x1cc, 0);
}

/* Riattraversa clock, PHY e timing con le tuple PLL del modo indicato. */
static int apply_mode(const struct video_mode *mode) {
    stage = 1;
    active_mode = mode;

    int rc = display_clocks_on();
    if (rc != DISPLAY_OK) return rc;
    rc = display_phy_on();
    if (rc != DISPLAY_OK) return rc;
    return display_start(mode->width, mode->height);
}

int display_set_mode(uint32_t width, uint32_t height) {
    const struct video_mode *mode = mode_for(width, height);
    if (!mode) return DISPLAY_ERR_MODE;
    if (mode == active_mode && stage >= 4) return DISPLAY_OK;

    const struct video_mode *previous = active_mode;
    if (stage >= 4) display_output_off();
    services_delay_ms(20);

    int rc = apply_mode(mode);
    if (rc != DISPLAY_OK && previous && previous != mode) {
        /* Un cambio fallito non deve lasciare lo schermo nero: si torna al
         * modo precedente, che era gia' funzionante. */
        os_puts("[display] switch failed, restoring the previous mode\n");
        if (apply_mode(previous) != DISPLAY_OK) {
            os_puts("[display] restore failed: output off\n");
        }
        return rc;
    }
    return rc;
}

void display_fill(uint32_t color) {
    volatile uint32_t *fb = display_buffer();
    if (!active_mode) return;
    uint32_t count = active_mode->width * active_mode->height;
    for (uint32_t i = 0; i < count; i++) {
        fb[i] = color;
    }
    mmio_fence();
}

/*
 * Carta di prova.
 *
 * - otto barre verticali: verificano l'ordine dei canali e la geometria
 *   orizzontale (devono essere otto, larghe 80 px, senza scalature);
 * - un gradiente orizzontale in basso: verifica lo stride. Se lo stride fosse
 *   sbagliato, le righe del gradiente risulterebbero inclinate o spezzate;
 * - un bordo grigio di 2 px: verifica che l'area visibile coincida con
 *   640x480, senza tagli o avvolgimenti.
 */
void display_test_pattern(void) {
    static const uint32_t bars[8] = {
        0xFFFFFFFFu, 0xFFFFFF00u, 0xFF00FFFFu, 0xFF00FF00u,
        0xFFFF00FFu, 0xFFFF0000u, 0xFF0000FFu, 0xFF000000u
    };
    if (!active_mode) return;
    const uint32_t w = active_mode->width;
    const uint32_t h = active_mode->height;
    const uint32_t bar_w = w / 8u;
    const uint32_t bars_h = h - 32u;
    volatile uint32_t *fb = display_buffer();

    for (uint32_t y = 0; y < bars_h; y++) {
        volatile uint32_t *row = fb + (uintptr_t)y * w;
        for (uint32_t x = 0; x < w; x++) {
            row[x] = bars[x / bar_w];
        }
    }

    for (uint32_t y = bars_h; y < h; y++) {
        volatile uint32_t *row = fb + (uintptr_t)y * w;
        for (uint32_t x = 0; x < w; x++) {
            uint32_t v = (x * 255u) / (w - 1u);
            row[x] = 0xFF000000u | (v << 16) | (v << 8) | v;
        }
    }

    for (uint32_t y = 0; y < h; y++) {
        volatile uint32_t *row = fb + (uintptr_t)y * w;
        for (uint32_t t = 0; t < 2u; t++) {
            row[t] = 0xFF808080u;
            row[w - 1u - t] = 0xFF808080u;
        }
    }
    for (uint32_t t = 0; t < 2u; t++) {
        volatile uint32_t *top = fb + (uintptr_t)t * w;
        volatile uint32_t *bottom = fb + (uintptr_t)(h - 1u - t) * w;
        for (uint32_t x = 0; x < w; x++) {
            top[x] = 0xFF808080u;
            bottom[x] = 0xFF808080u;
        }
    }

    mmio_fence();
}

int display_bringup(uint32_t width, uint32_t height, uint32_t color) {
    int rc;

    /* Il PHY legge le tuple PLL dal modo attivo: va scelto prima. */
    active_mode = mode_for(width, height);
    if (!active_mode) return DISPLAY_ERR_MODE;

    os_puts("[display] 1/5 power VOUT\n");
    rc = display_power_on();
    if (rc != DISPLAY_OK) return rc;

    os_puts("[display] 2/5 clock, reset, routing, identity\n");
    rc = display_clocks_on();
    if (rc != DISPLAY_OK) return rc;
    os_puts("[display]      DC revision=");
    os_puthex64(identity_revision);
    os_puts(" model=");
    os_puthex64(identity_model);
    os_puts(" customer=");
    os_puthex64(identity_customer);
    os_puts("\n");

    os_puts("[display] 3/5 PHY TMDS\n");
    rc = display_phy_on();
    if (rc != DISPLAY_OK) return rc;

    /*
     * Ordine fedele al percorso validato: il buffer si riempie PRIMA di
     * avviare lo scanout. Il DC legge da 0x70000000; se la sequenza parte
     * con memoria non inizializzata il pannello puo' agganciarsi su nero.
     */
    os_puts("[display] 4/5 filling framebuffer\n");
    display_fill(color);

    os_puts("[display] 5/5 timing and scanout\n");
    rc = display_start(width, height);
    if (rc != DISPLAY_OK) return rc;

    /* Conferma che il controller punta davvero al buffer e che la memoria
     * contiene il colore scritto. */
    os_puts("[display]      fb_base=");
    os_puthex64(read32(DC + 0x1400));
    os_puts(" stride=");
    os_putu32(read32(DC + 0x1408));
    os_puts(" fb[0]=");
    os_puthex64(display_buffer()[0]);
    os_puts("\n");

    return DISPLAY_OK;
}

volatile uint32_t *display_buffer(void) {
    return (volatile uint32_t *)(uintptr_t)MARSFB_UNCACHED;
}

uint32_t display_width(void)  { return active_mode ? active_mode->width : 0; }
uint32_t display_height(void) { return active_mode ? active_mode->height : 0; }

uintptr_t display_failed_reg(void) { return failed_reg; }
uint32_t  display_expected(void)   { return expected_value; }
uint32_t  display_observed(void)   { return observed_value; }
