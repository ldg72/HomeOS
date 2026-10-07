#ifndef EXEC_DEVICES_H
#define EXEC_DEVICES_H

#include <exec/exec.h>

/*
 * Device Structure
 * In Exec64, a Device is essentially a Library with additional
 * specific fields for I/O handling, but for now we keep it
 * simple as a wrapper around struct Library.
 */
struct IORequest;

struct Device {
    struct Library dd_Library;
    void *dd_MainInterface;
    int32_t (*dd_DoIO)(struct IORequest *request);
};

/*
 * IORequest Structure
 * Used for communication with devices.
 */
struct IORequest {
    struct Message io_Message;
    struct Device *io_Device;
    uint32_t       io_Command;
    uint32_t       io_Flags;
    int32_t        io_Error;
    uint32_t       io_Actual;
    uint32_t       io_Length;
    void          *io_Data;
    uint32_t       io_Offset;
};

// Device Commands
#define CMD_INVALID 0
#define CMD_RESET   1
#define CMD_READ    2
#define CMD_WRITE   3
#define CMD_UPDATE  4
#define CMD_CLEAR   5
#define CMD_STOP    6
#define CMD_START   7
#define CMD_FLUSH   8
#define CMD_NONSTD  0x8000

#endif
