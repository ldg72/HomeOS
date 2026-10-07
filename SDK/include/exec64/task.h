#ifndef EXEC64_UAPI_TASK_H
#define EXEC64_UAPI_TASK_H

#include <stdint.h>

#ifndef EXEC64_TASK_HANDLE_DEFINED
#define EXEC64_TASK_HANDLE_DEFINED
typedef uint64_t Exec64TaskHandle;
#endif

#define EXEC64_TASK_HANDLE_INVALID 0ULL

#define EXEC64_TASK_WATCH_V1_SIZE     16u
#define EXEC64_TASK_EXIT_INFO_V1_SIZE 24u

#define EXEC64_TASK_EXIT_NONE      0u
#define EXEC64_TASK_EXIT_RETURN    1u
#define EXEC64_TASK_EXIT_SYS_EXIT  2u
#define EXEC64_TASK_EXIT_EXCEPTION 3u
#define EXEC64_TASK_EXIT_REMOVED   4u

#define EXEC64_TASK_SUPERVISE_OK    0
#define EXEC64_TASK_SUPERVISE_ERROR (-1)

#define EXEC64_TASK_TAKE_PENDING 0
#define EXEC64_TASK_TAKE_READY   1
#define EXEC64_TASK_TAKE_ERROR   (-1)

struct Exec64TaskWatch {
    uint32_t etw_StructSize;
    uint32_t etw_SignalMask;
    Exec64TaskHandle etw_Handle;
};

struct Exec64TaskExitInfo {
    uint32_t eti_StructSize;
    uint32_t eti_Reason;
    uint64_t eti_Detail;
    uint64_t eti_Reserved;
};

_Static_assert(sizeof(struct Exec64TaskWatch) == EXEC64_TASK_WATCH_V1_SIZE,
               "Exec64TaskWatch v1 size mismatch");
_Static_assert(sizeof(struct Exec64TaskExitInfo) ==
                   EXEC64_TASK_EXIT_INFO_V1_SIZE,
               "Exec64TaskExitInfo v1 size mismatch");

#endif
