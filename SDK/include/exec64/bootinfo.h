#ifndef EXEC64_UAPI_BOOTINFO_H
#define EXEC64_UAPI_BOOTINFO_H

#include <stdint.h>

#define EXEC64_BOOTINFO_MAGIC       0x45583634424F4F54ULL
#define EXEC64_BOOTINFO_ABI_MAJOR   1u
#define EXEC64_BOOTINFO_ABI_MINOR   2u
#define EXEC64_BOOTINFO_V1_0_SIZE   64u
#define EXEC64_BOOTINFO_V1_1_SIZE   96u
#define EXEC64_BOOTINFO_V1_2_SIZE   112u
#define EXEC64_BOOTINFO_V1_SIZE     EXEC64_BOOTINFO_V1_2_SIZE

#define EXEC64_BOOTINFO_HAS_PLATFORM_DATA       (1ULL << 0)
#define EXEC64_BOOTINFO_HAS_SYSTEM_IMAGE        (1ULL << 1)
#define EXEC64_BOOTINFO_SYSTEM_IMAGE_WRITABLE   (1ULL << 2)
#define EXEC64_BOOTINFO_HAS_INIT_IMAGE          (1ULL << 3)

#define EXEC64_BOOTINFO_HAS_CATALOG             (1ULL << 4)

#define EXEC64_BOOTINFO_IMAGE_NONE   0u
#define EXEC64_BOOTINFO_IMAGE_EXFAT  1u
#define EXEC64_BOOTINFO_IMAGE_ELF64  2u

#define EXEC64_INIT_ABI_MAJOR 1u
#define EXEC64_INIT_ABI_MINOR 2u

struct Exec64BootInfo {
    uint64_t bi_Magic;
    uint16_t bi_AbiMajor;
    uint16_t bi_AbiMinor;
    uint32_t bi_StructSize;
    uint64_t bi_Flags;
    uint64_t bi_PlatformData;
    uint64_t bi_SystemImageStart;
    uint64_t bi_SystemImageSize;
    uint32_t bi_SystemImageType;
    uint32_t bi_Reserved0;
    uint64_t bi_Reserved1;
    uint64_t bi_InitImageStart;
    uint64_t bi_InitImageSize;
    uint32_t bi_InitImageType;
    uint16_t bi_InitAbiMajor;
    uint16_t bi_InitAbiMinor;
    uint64_t bi_Reserved2;
    uint64_t bi_CatalogStart;
    uint64_t bi_CatalogSize;
};

_Static_assert(sizeof(struct Exec64BootInfo) == EXEC64_BOOTINFO_V1_2_SIZE,
               "Exec64BootInfo v1.2 size mismatch");

#endif
