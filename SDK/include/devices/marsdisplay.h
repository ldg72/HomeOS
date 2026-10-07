#ifndef DEVICES_MARSDISPLAY_H
#define DEVICES_MARSDISPLAY_H
#include <devices/display.h>
#include <exec64/marsfb.h>

#define MARS_DISPLAY_DEVICE "jh7110display.device"
/* Device-specific diagnostic. DisplayRequest: SurfaceId=operation, Width=value;
 * io_Data/Length describe MarsFbReport. Ordinary clients use DISPLAY_CMD_*.
 * Mutating diagnostics are refused while a display client owns the scanout. */
#define MARS_DISPLAY_CMD_DIAGNOSTIC 0x7e01u
#endif
