#ifndef DEVICES_INPUT_H
#define DEVICES_INPUT_H

#include <exec/devices.h>

#define INPUT_CMD_READ_EVENT (CMD_NONSTD + 1)
#define INPUT_CMD_READ_EVENTS (CMD_NONSTD + 2)

#define INPUT_EVENT_SYN 0x00
#define INPUT_EVENT_KEY 0x01
#define INPUT_EVENT_REL 0x02
#define INPUT_EVENT_ABS 0x03

#define INPUT_REL_X 0x00
#define INPUT_REL_Y 0x01
#define INPUT_ABS_X 0x00
#define INPUT_ABS_Y 0x01

#define INPUT_READ_OK    0
#define INPUT_READ_EMPTY 1

struct ExecInputEvent {
    uint16_t eie_Type;
    uint16_t eie_Code;
    uint32_t eie_Value;
};

struct InputDeviceRequest {
    struct IORequest idr_IO;
    struct ExecInputEvent idr_Event;
};

/*
 * INPUT_CMD_READ_EVENTS uses a plain IORequest. io_Data points to an array of
 * ExecInputEvent, io_Length is its size in bytes and io_Actual reports the
 * number of bytes written. The command returns INPUT_READ_EMPTY when no event
 * is queued.
 */

#endif
