#ifndef EXEC_LIB_H
#define EXEC_LIB_H

#include <stdint.h>
#include <stddef.h>
#include <exec/exec.h>

#ifndef EXEC64_TASK_HANDLE_DEFINED
#define EXEC64_TASK_HANDLE_DEFINED
typedef uint64_t Exec64TaskHandle;
#endif

struct Exec64TaskWatch;
struct Exec64TaskExitInfo;

// --- EXEC INTERFACE DEFINITION ---
// Moved from exec.h to separate definition from implementation details.

struct ExecInterface {
    struct Interface i; // Base interface metadata

    // Function Pointers (The LVOs in C form)
    void     (*NewList)(struct List *list);
    void     (*AddHead)(struct List *list, struct Node *node);
    void     (*AddTail)(struct List *list, struct Node *node);
    void     (*Insert)(struct List *list, struct Node *node, struct Node *listNode);
    void     (*Enqueue)(struct List *list, struct Node *node);
    void     (*MoveList)(struct List *destination, struct List *source);
    void     (*Remove)(struct Node *node);
    struct Node* (*RemHead)(struct List *list);
    struct Node* (*RemTail)(struct List *list);
    struct Node* (*FindName)(struct List *list, const char *name);
    struct Node* (*GetHead)(struct List *list);
    struct Node* (*GetTail)(struct List *list);
    struct Node* (*GetSucc)(struct Node *node);
    struct Node* (*GetPred)(struct Node *node);
    void*     (*AllocMem)(uint32_t size, uint32_t attributes);
    void     (*FreeMem)(void *ptr, uint32_t size);
    uint32_t (*AvailMem)(uint32_t attributes);
    void     (*DebugPutS)(const char *s);
    void     (*DebugPutC)(char c);
    void     (*Halt)(void);

    // Messaging
    void     (*PutMsg)(struct MsgPort *port, struct Message *msg);
    struct Message* (*GetMsg)(struct MsgPort *port);
    void     (*ReplyMsg)(struct Message *msg);
    struct Message* (*WaitPort)(struct MsgPort *port);
    struct MsgPort* (*CreateMsgPort)(void);
    void     (*DeleteMsgPort)(struct MsgPort *port);

    // Tasks
    void     (*AddTask)(struct Task *task, struct ExecCPU *targetCPU);
    void     (*RemTask)(struct Task *task);
    struct Task* (*FindTask)(const char *name);
    void     (*SetExcept)(void (*handler)(void *), uint32_t flags);
    void*    (*PrepareStack)(struct Task *task, void (*code)(void), void *arg);

    // Interrupts
    void     (*AddIntServer)(uint32_t irq, struct Interrupt *is);
    void     (*RemIntServer)(uint32_t irq, struct Interrupt *is);
    void     (*EnableIRQ)(uint32_t irq);
    void     (*DisableIRQ)(uint32_t irq);

    // Signals
    void     (*Signal)(struct Task *task, uint32_t sigSet);
    uint32_t (*SetSignal)(uint32_t newSignals, uint32_t signalMask);
    void     (*Forbid)(void);
    void     (*Permit)(void);
    uint32_t (*Wait)(uint32_t sigSet);
    int8_t   (*AllocSignal)(int8_t signalNum);
    void     (*FreeSignal)(int8_t signalNum);

    // Libraries
    struct Library*   (*OpenLibrary)(const char *name, uint32_t version);
    void              (*CloseLibrary)(struct Library *lib);
    struct Interface* (*GetInterface)(struct Library *lib, const char *name, uint32_t version, void *taglist);
    void              (*AddLibrary)(struct Library *lib);
    void              (*RemLibrary)(struct Library *lib);

    // Devices
    struct Device*    (*OpenDevice)(const char *name, uint32_t version);
    void              (*CloseDevice)(struct Device *dev);

    // Interfaces (Modular)
    void              (*AddInterface)(struct Library *lib, struct Interface *iface);
    void              (*RemInterface)(struct Library *lib, struct Interface *iface);
    void              (*DropInterface)(struct Interface *iface);

    // Semaphores
    void (*InitSemaphore)(struct SignalSemaphore *sigSem);
    void (*ObtainSemaphore)(struct SignalSemaphore *sigSem);
    void (*ReleaseSemaphore)(struct SignalSemaphore *sigSem);

    // Time management (New)
    uint64_t (*GetTicks)(void);
    uint32_t (*GetTicksMs)(void);
    void (*DelayMs)(uint32_t ms);

    // Security (New)
    int (*ValidatePtr)(const void *ptr, uint32_t size);
    int (*MakeExecutable)(void *ptr, uint32_t size);

    // Memory Pools (New)
    void* (*CreatePool)(uint32_t flags, uint32_t puddleSize, uint32_t threshSize);
    void  (*DeletePool)(void *poolHeader);
    void* (*AllocPooled)(void *poolHeader, uint32_t size);
    void  (*FreePooled)(void *poolHeader, void *memory, uint32_t size);

    // Libraries extension
    struct Task* (*GetCurrentTask)(void);

    // Append-only task supervision extension
    int32_t (*AddSupervisedTask)(struct Task *task,
                                 struct Exec64TaskWatch *watch);
    int32_t (*TakeTaskExit)(Exec64TaskHandle handle,
                            struct Exec64TaskExitInfo *info);
};

#endif // EXEC_LIB_H
