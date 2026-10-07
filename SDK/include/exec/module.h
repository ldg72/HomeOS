#ifndef EXEC64_MODULE_H
#define EXEC64_MODULE_H

#include <stdint.h>
#include <exec/exec.h>

/* Versioned, fixed-width transport; no caller-supplied privileged entrypoints. */
#define EXEC_MODULE_ABI 1u
#define EXEC_MODULE_LIBRARY 1u
#define EXEC_MODULE_SUPERVISOR 0u
#define EXEC_MODULE_USER 1u
#define EXEC_MODULE_DIRECT 1u
#define EXEC_MODULE_PROXY 2u
#define EXEC_MODULE_PAYLOAD 128u
#define EXEC_MODULE_OK 0
#define EXEC_MODULE_BAD_ARGUMENT (-1)
#define EXEC_MODULE_BAD_ABI (-2)
#define EXEC_MODULE_DENIED (-3)
#define EXEC_MODULE_UNSUPPORTED (-4)
#define EXEC_MODULE_NO_MEMORY (-5)
#define EXEC_MODULE_INIT_FAILED (-6)
#define EXEC_MODULE_BAD_HANDLE (-7)
#define EXEC_MODULE_BUSY (-8)
#define EXEC_MODULE_BAD_OPERATION (-9)
#define EXEC_MODULE_BAD_BUFFER (-10)
#define EXEC_MODULE_BAD_VERSION (-11)

struct ExecModuleOpen {
    uint32_t size, abi, object_type, requested_mode;
    uint32_t min_version, flags, reserved[2];
    char name[32];
    /* Outputs; input values in this part are never authoritative. */
    uint64_t handle, direct_interface;
    uint32_t actual_mode, binding;
    uint16_t library_version, library_revision;
    uint32_t reserved_out;
    char id[32];
};

struct ExecModuleCall {
    uint32_t size, abi, operation, input_size;
    uint64_t handle;
    uint32_t output_capacity, output_size;
    uint8_t input[EXEC_MODULE_PAYLOAD];
    uint8_t output[EXEC_MODULE_PAYLOAD];
};

/* Descriptor returned by the existing 32-byte UserLibInitCtx bootstrap.
 * Dispatch is inspected/called only for a trusted SUPERVISOR image. */
typedef int32_t (*ExecModuleDispatch)(uint32_t operation,
    const uint8_t *input, uint32_t input_size,
    uint8_t *output, uint32_t capacity, uint32_t *output_size);

struct ExecModuleDescriptor {
    struct Library library;
    struct Interface *main_interface;
    uint32_t size, abi;
    ExecModuleDispatch dispatch;
};

/* Explicit proxy bootstrap, never passed to a legacy entrypoint by guessing.
 * The first 32 bytes retain UserLibInitCtx; the full proxy contract is v1. */
struct ExecModuleProxyInit {
    struct UserLibInitCtx library;
    uint32_t size, abi;
    uint64_t handle;
    uint32_t interface_version, interface_size;
    uint64_t reserved;
    char name[32];
};
_Static_assert(sizeof(struct ExecModuleProxyInit) == 96, "proxy bootstrap ABI");
#endif
