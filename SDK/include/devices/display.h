#ifndef DEVICES_DISPLAY_H
#define DEVICES_DISPLAY_H

#include <exec/devices.h>

/* Display I/O v1. The first five commands retain the VirtIO GPU wire ABI.
 * Applications use graphics.library; these requests are for display clients.
 * A backend owns hardware discovery, DMA/cache maintenance and scanout.
 */
#define DISPLAY_CMD_SETUP_SCANOUT (CMD_NONSTD + 1)
#define DISPLAY_CMD_PRESENT       (CMD_NONSTD + 2)
#define DISPLAY_CMD_RELEASE       (CMD_NONSTD + 3)
#define DISPLAY_CMD_MOVE_CURSOR   (CMD_NONSTD + 4)
#define DISPLAY_CMD_SET_VISIBLE   (CMD_NONSTD + 5)
#define DISPLAY_CMD_QUERY_CAPS    (CMD_NONSTD + 6)
#define DISPLAY_CMD_QUERY_MODE    (CMD_NONSTD + 7)

#define DISPLAY_ABI_VERSION 1u
#define DISPLAY_FORMAT_XRGB8888 1u /* uint32_t 0xXXRRGGBB; scanout ignores X */
#define DISPLAY_CAP_VARIABLE_SIZE (1u << 0)
#define DISPLAY_CAP_ASYNC_PRESENT (1u << 1)
#define DISPLAY_CAP_CURSOR_MOVE   (1u << 2)

struct DisplayRequest {
    struct IORequest dr_IO;
    uint32_t dr_SurfaceId;
    uint32_t dr_Width;
    uint32_t dr_Height;
    int32_t  dr_X;
    int32_t  dr_Y;
    uint32_t dr_RectWidth;
    uint32_t dr_RectHeight;
};

struct DisplayMode {
    uint32_t dm_Width;
    uint32_t dm_Height;
    uint32_t dm_RefreshMilliHz; /* zero: unspecified/virtual */
    uint32_t dm_StrideBytes;
};

struct DisplayCaps {
    uint32_t dc_ABIVersion;
    uint32_t dc_Size;
    uint32_t dc_Flags;
    uint32_t dc_Format;
    struct DisplayMode dc_PreferredMode;
    uint32_t dc_MinWidth;
    uint32_t dc_MinHeight;
    uint32_t dc_MaxWidth;
    uint32_t dc_MaxHeight;
    uint32_t dc_ModeCount; /* zero for a variable-size virtual display */
    uint32_t dc_Reserved[3];
};

/* QUERY_CAPS: io_Data -> DisplayCaps, io_Length == sizeof(DisplayCaps).
 * QUERY_MODE: io_Offset is an index < dc_ModeCount, io_Data -> DisplayMode,
 *             io_Length == sizeof(DisplayMode).
 * Queries report driver-supported modes, not EDID/link/scanout status. They
 * must not acquire a scanout, program hardware, or change its current owner.
 * SETUP: nonzero surface id; io_Data is a tightly packed width*height*4 buffer.
 * The caller retains that buffer until RELEASE succeeds; PRESENT may be async.
 * PRESENT: dirty rectangle; SET_VISIBLE: dr_Width 0/1, retains the backing.
 * There is no vblank, page-flip or completion-fence guarantee in v1.
 * Return 0 on success, negative on failure; -2 on SET_VISIBLE means retry.
 * QUERY commands set io_Actual to the result size on success, zero on failure.
 */

#endif
