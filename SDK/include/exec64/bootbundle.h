#ifndef EXEC64_UAPI_BOOTBUNDLE_H
#define EXEC64_UAPI_BOOTBUNDLE_H

#include <stdint.h>

#define EXEC64_BOOT_BUNDLE_MAGIC       0x4C444E4234365845ULL
#define EXEC64_BOOT_BUNDLE_ABI_MAJOR   1u
#define EXEC64_BOOT_BUNDLE_ABI_MINOR   1u
#define EXEC64_BOOT_BUNDLE_V1_0_SIZE 64u
#define EXEC64_BOOT_BUNDLE_V1_1_SIZE 80u
#define EXEC64_BOOT_BUNDLE_HEADER_SIZE EXEC64_BOOT_BUNDLE_V1_1_SIZE
#define EXEC64_BOOT_BUNDLE_ALIGNMENT   4096u

#define EXEC64_BOOT_PAYLOAD_NONE   0u
#define EXEC64_BOOT_PAYLOAD_ELF64  1u
#define EXEC64_BOOT_PAYLOAD_EXFAT  2u

struct Exec64BootBundleHeader {
    uint64_t bb_Magic;
    uint16_t bb_AbiMajor;
    uint16_t bb_AbiMinor;
    uint32_t bb_HeaderSize;
    uint64_t bb_BundleSize;
    uint64_t bb_InitOffset;
    uint64_t bb_InitSize;
    uint64_t bb_SystemOffset;
    uint64_t bb_SystemSize;
    uint32_t bb_InitType;
    uint32_t bb_SystemType;
    uint64_t bb_CatalogOffset;
    uint64_t bb_CatalogSize;
};

_Static_assert(sizeof(struct Exec64BootBundleHeader) ==
                   EXEC64_BOOT_BUNDLE_HEADER_SIZE,
               "Exec64 boot bundle header size mismatch");

#endif
