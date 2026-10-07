#ifndef EXEC64_UAPI_INIT_H
#define EXEC64_UAPI_INIT_H

#include <stdint.h>
#include <exec64/bootresource.h>

#define EXEC64_INIT_CONTEXT_MAGIC       0x45583634494E4954ULL
#define EXEC64_INIT_CONTEXT_ABI_MAJOR   1u
#define EXEC64_INIT_CONTEXT_ABI_MINOR   2u
#define EXEC64_INIT_CONTEXT_V1_0_SIZE   64u
#define EXEC64_INIT_CONTEXT_V1_1_SIZE   80u
#define EXEC64_INIT_CONTEXT_V1_2_SIZE   96u
#define EXEC64_INIT_CONTEXT_V1_SIZE     EXEC64_INIT_CONTEXT_V1_2_SIZE

#define EXEC64_INIT_HAS_BOOT_RESOURCE (1ULL << 0)

#define EXEC64_INIT_SUPERVISOR (1ULL << 1)
struct ExecBase;
/* Only delivered to supervisor Init. Bound once; no trap or USER proxy. */
struct Exec64InitServices {
    uint32_t size;
    uint32_t version;
    int (*ResourceInfo)(Exec64BootResourceHandle, struct Exec64BootResourceInfo *);
    int (*ResourceRead)(Exec64BootResourceHandle, uint64_t, void *, uint32_t);
    int (*ResourceWrite)(Exec64BootResourceHandle, uint64_t, const void *, uint32_t);
    void *(*LoadImage)(const void *, uint64_t);
    int (*UnloadImage)(void *);
};
struct Exec64InitContext {
    uint64_t ic_Magic;
    uint16_t ic_AbiMajor;
    uint16_t ic_AbiMinor;
    uint32_t ic_StructSize;
    uint64_t ic_Flags;
    uint64_t ic_Reserved0;
    uint64_t ic_Reserved1;
    uint64_t ic_Reserved2;
    uint64_t ic_Reserved3;
    uint64_t ic_Reserved4;
    Exec64BootResourceHandle ic_BootResource;
    uint64_t ic_Reserved5;
    struct ExecBase *ic_SysBase;
    const struct Exec64InitServices *ic_Services;
};

_Static_assert(sizeof(struct Exec64InitContext) == EXEC64_INIT_CONTEXT_V1_SIZE,
               "Exec64InitContext v1 size mismatch");

#endif
