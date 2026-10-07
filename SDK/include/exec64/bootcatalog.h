#ifndef EXEC64_UAPI_BOOTCATALOG_H
#define EXEC64_UAPI_BOOTCATALOG_H
#include <stdint.h>
#define EXEC64_CATALOG_MAGIC 0x54414334365845ULL
#define EXEC64_CATALOG_ABI_MAJOR 1u
#define EXEC64_CATALOG_ABI_MINOR 0u
#define EXEC64_CATALOG_MAX_ENTRIES 32u
#define EXEC64_CATALOG_MAX_SIZE (32u * 1024u * 1024u)
#define EXEC64_CATALOG_MAX_IMAGE (2u * 1024u * 1024u)
#define EXEC64_CATALOG_MAX_LOAD 0xfe000u
#define EXEC64_CATALOG_USER 1u
#define EXEC64_CATALOG_SUPERVISOR 2u
#define EXEC64_CATALOG_LIBRARY 1u
#define EXEC64_CATALOG_INIT_DESCRIPTOR 1u
#define EXEC64_CATALOG_INIT_LEGACY 2u
#define EXEC64_CATALOG_PROXY_V1 1u
#define EXEC64_CATALOG_RESIDENT 1u
struct Exec64BootCatalogHeader {
    uint64_t magic;
    uint16_t major, minor;
    uint32_t header_size;
    uint64_t total_size;
    uint32_t entry_size, count, flags, reserved0;
    uint64_t reserved[3];
};
struct Exec64BootCatalogEntry {
    char name[32], interface_name[32];
    uint32_t type, domains, protect_domain, performance_domain;
    uint16_t version, revision, machine, bootstrap;
    uint32_t interface_version, interface_size, protocol, lifetime;
    uint64_t image_offset, image_size, proxy_offset, proxy_size;
    uint8_t image_sha256[32], proxy_sha256[32];
    uint32_t proxy_bootstrap, descriptor_abi;
    uint64_t reserved[6];
};
_Static_assert(sizeof(struct Exec64BootCatalogHeader) == 64, "catalog header ABI");
_Static_assert(sizeof(struct Exec64BootCatalogEntry) == 256, "catalog entry ABI");
#endif
