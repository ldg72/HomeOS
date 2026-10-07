#ifndef EXEC64_MARSFB_H
#define EXEC64_MARSFB_H

#include <stdint.h>

/* Experimental, explicit board diagnostic; not a display device ABI.
 * Transport: MARS_DISPLAY_CMD_DIAGNOSTIC in jh7110display.device.
 * SYS_MARSFB_DIAG is retired; no user MMIO/DMA address is accepted.
 * Unsupported platforms return MARSFB_UNSUPPORTED without accessing hardware. */
#define MARSFB_ABI 2u
#define MARSFB_STATUS 0u
#define MARSFB_POWER 1u
#define MARSFB_CLOCKS 2u
#define MARSFB_PHY 3u
#define MARSFB_FILL_BEGIN 4u
#define MARSFB_FILL_STEP 5u
#define MARSFB_START 6u
#define MARSFB_OFF 7u
#define MARSFB_LATCH_RELEASE 8u
#define MARSFB_LIVE_BEGIN 9u
#define MARSFB_LIVE_STEP 10u
#define MARSFB_DEMO_BEGIN 11u
#define MARSFB_DEMO_STEP 12u
#define MARSFB_DEMO_FRAME 13u

/* Output left running for observation, but no scan motion was confirmed. */
#define MARSFB_UNCONFIRMED 1

#define MARSFB_UNSUPPORTED (-1)
#define MARSFB_BAD_ARGUMENT (-2)
#define MARSFB_BAD_STATE (-3)
#define MARSFB_TIMEOUT (-4)
#define MARSFB_HARDWARE (-5)
#define MARSFB_BUSY (-6)

/* stage: 0 untouched, 1 powered, 2 clocks/reset ready, 3 PHY ready,
 *        4 framebuffer complete, 5 scanout requested. */
struct MarsFbReport {
    uint32_t abi, size, stage, rows;
    uint32_t failed_reg, expected, observed, pmu;
    uint32_t sys_noc_clock, sys_axi_div, sys_reset0, sys_reset1;
    uint32_t vout_reset, pixel_mux, route, panel_select;
    uint32_t dc_model, dc_revision, dc_customer, pre_lock;
    uint32_t post_lock, hpd, fb_address, fb_stride;
    uint32_t fb_config, panel_config, panel_start, location;
    uint32_t panel_config_ex, fb_format_config, scan_initial, scan_motion;
    uint32_t dc_axi_clock, dc_core_clock, dc_ahb_clock, lcd_clock;
};
_Static_assert(sizeof(struct MarsFbReport) == 144, "MarsFbReport ABI");

#endif
