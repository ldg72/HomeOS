#ifndef EXEC64_UAPI_BOOTRESOURCE_H
#define EXEC64_UAPI_BOOTRESOURCE_H

#include <stdint.h>

typedef uint64_t Exec64BootResourceHandle;

#define EXEC64_BOOT_RESOURCE_INVALID 0ULL

#define EXEC64_BOOT_RESOURCE_ABI_MAJOR 1u
#define EXEC64_BOOT_RESOURCE_ABI_MINOR 0u

#define EXEC64_BOOT_RESOURCE_TYPE_BLOCK 1u

#define EXEC64_BOOT_RESOURCE_FORMAT_NONE  0u
#define EXEC64_BOOT_RESOURCE_FORMAT_EXFAT 1u

#define EXEC64_BOOT_RESOURCE_READABLE      (1u << 0)
#define EXEC64_BOOT_RESOURCE_WRITABLE      (1u << 1)
#define EXEC64_BOOT_RESOURCE_MEMORY_BACKED (1u << 2)

#define EXEC64_BOOT_RESOURCE_INFO_V1_SIZE 48u
#define EXEC64_BOOT_RESOURCE_MAX_TRANSFER (64u * 1024u)

struct Exec64BootResourceInfo {
    uint16_t bri_AbiMajor;
    uint16_t bri_AbiMinor;
    uint32_t bri_StructSize;
    uint32_t bri_Type;
    uint32_t bri_Format;
    uint32_t bri_Flags;
    uint32_t bri_BlockSize;
    uint64_t bri_ByteSize;
    uint64_t bri_Reserved0;
    uint64_t bri_Reserved1;
};

_Static_assert(sizeof(struct Exec64BootResourceInfo) ==
                   EXEC64_BOOT_RESOURCE_INFO_V1_SIZE,
               "Exec64BootResourceInfo v1 size mismatch");

#endif
