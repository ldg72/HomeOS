#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

struct ExecCPU;
struct Device;
struct ExecBase;

struct Node {
    struct Node *ln_Succ;
    struct Node *ln_Pred;
    uint8_t      ln_Type;
    int8_t       ln_Pri;
    char        *ln_Name;
};

struct List {
    struct Node *lh_Head;
    struct Node *lh_Tail;
    struct Node *lh_TailPred;
};

/*
 * Library Structure (Base)
 * Ogni libreria in Exec64 deve iniziare con questa struttura.
 */
struct Library {
    struct Node lib_Node;
    uint16_t    lib_Version;
    uint16_t    lib_Revision;
    uint8_t     lib_Flags;
    uint8_t     lib_pad[3];  // Pad to 4 bytes
    char       *lib_IdString;
    uint32_t    lib_OpenCnt;
    uint32_t    lib_pad2;    // Pad to 8 bytes for pointer alignment
};

#define LIBF_USERMODE (1u << 7)

/*
 * Interface Structure (Base)
 * Contiene i dati necessari per gestire un'interfaccia OS4-style.
 */
struct Interface {
    struct Library *if_Library;
    uint32_t        if_Data;
};

struct UserLibInitCtx {
    struct ExecBase *LocalSysBase;
    uint32_t         LoadMode;
    uint32_t         ObjectType;
    const char      *Name;
    uint32_t         Version;
    uint32_t         Reserved0;
};

struct Task {
    struct Node tc_Node;
    uint8_t     tc_Flags;
    uint8_t     tc_State;
    int8_t      tc_Priority;
    uint8_t     tc_Reserved1[5];
    void        *tc_StackBase;
    void        *tc_StackPointer;
    char        *tc_Name;
    uint32_t    tc_SigWait;   // Segnali su cui il task è in attesa
    uint32_t    tc_SigRecvd;  // Segnali ricevuti
    uint32_t    tc_SigAlloc;  // Segnali allocati (1 = occupato)
    uint32_t    tc_CpuID;     // ID del Core su cui gira il task
    /* Fixed public layout on both CPUs. These slots are Core-owned. */
    union {
        uint64_t tc_Reserved3[2];
        struct {
            void *tc_KernelStackBase;
            uint64_t tc_ArchReserved;
        };
    };
    uint64_t    tc_Registers[33];      // Callee-saved RISC-V registers (s0-s11, ra) + padding
                                       // [0]=s1 [1-10]=s2-s11 [11]=ra [12]=s0/fp
    uint64_t    tc_WakeupTick;         // Tick al quale svegliare il task (0 = non in sleep)

    // Exception Handling (New Phase 3)
    void      (*tc_ExceptCode)(void *);
    void       *tc_ExceptData;
    uint32_t    tc_ExceptFlags;

    // FPU Context (RISC-V D extension, lazy save/restore)
    // Callee-saved: fs0-fs11 (12 x 64-bit) + fcsr (1 x 64-bit) = 13 slots used
    // Array oversized for alignment; active entries: indices 0-11 = fs0-fs11
    uint64_t    tc_FPURegisters[64];
    uint64_t    tc_FCSR;               // fcsr (Floating-Point Control and Status Register)
    uint64_t    tc_FPU_pad;            // Reserved — padding for struct alignment
    uint16_t    tc_ASID;               // Sv39 Address Space ID for U-Mode tasks
    uint16_t    tc_AddressSpaceFlags;  // Exec64 MMU/task-space state flags
    uint32_t    tc_AddressSpaceGen;    // Reserved for future shootdown/versioning
    void       *tc_UserPageTable;      // Root page table for this task's U-Mode view
    
    struct Node tc_GlobalNode; // Node for ex_TaskGlobalList
    void       *tc_UserImageBase;      // Shared ELF/code image associated with this task
    uint32_t    tc_UserImageSize;
    uint32_t    tc_OwnedStackSize;     // Stack allocation size for deferred cleanup
    uint16_t    tc_ResourceFlags;      // Deferred cleanup/resource ownership flags
    uint16_t    tc_UserOwnedRangeCount; // Dynamic user-owned ranges tracked for this task
    uint16_t    tc_SharedRangeCount;   // Dynamic shared/public ranges currently mapped into this task
    uint16_t    tc_Reserved4;
    void       *tc_ExceptReturn;       // U-Mode recovery trampoline for SetExcept
    uint64_t    tc_ExceptContextRaw[36]; // User-visible ExceptionContext shadow copy
    uint32_t    tc_ExitReason;         // Termination reason recorded before task reap
    union {
        uint32_t tc_Reserved5; /* Legacy ABI alias; offsets and size unchanged. */
        /* Supervisor worker opt-in, configured after AddTask. Matching local
         * signals publish READY without requesting immediate preemption.
         * Remote wakeups and other signals retain normal Signal semantics. */
        uint32_t tc_SigDeferLocal;
    };
    uint64_t    tc_ExitDetail;         // Arch-specific detail (e.g. scause)
};

/*
 * Messaging System
 */
#include <exec/spinlock.h>

struct Message {
    struct Node     mn_Node;
    struct MsgPort *mn_ReplyPort;
    uint16_t        mn_Length;
};

struct MsgPort {
    struct Node     mp_Node;
    uint8_t         mp_Flags;
    uint8_t         mp_SigBit;
    struct Task    *mp_SigTask;
    struct List     mp_MsgList;
    spinlock_t      mp_Lock;     // Protects mp_MsgList
};

#define PA_SIGNAL 0
#define PA_SOFTINT 1
#define PA_IGNORE 2

#define MPF_ACTION_MASK 0x03
#define MPF_DEAD        0x40
#define MPF_OWNS_SIGBIT 0x80

/*
 * Signal Semaphores (AmigaOS style exclusive mutex)
 */
struct SignalSemaphore {
    struct Node     ss_Link;
    struct Task    *ss_Owner;
    int32_t         ss_NestCount;
    struct List     ss_WaitQueue;
    spinlock_t      ss_Lock;      // Protects semaphore structure in SMP
};

#define TS_READY   0
#define TS_RUNNING 1
#define TS_WAITING 2
#define TS_REMOVED 3
#define MAX_LINE   64

// CPU Flags for Forbid/Permit mechanism
#define CPUF_FORBID_MASK  0xFF00  // Upper 8 bits: Forbid nesting counter
#define CPUF_FORBID_SHIFT 8

#define SIG_VIRTIO (1 << 14) // Segnale riservato ai driver VirtIO
#define SIGB_TIMER 15        // Segnale per il Timer (wakeup)
#define SIGF_TIMER (1 << SIGB_TIMER)

#define SIGB_SEMAPHORE 13
#define SIGF_SEMAPHORE (1 << SIGB_SEMAPHORE) // Segnale riservato per i semafori

/*
 * Exception Types
 */
#define EXCB_DATA_ABORT 0
#define EXCF_DATA_ABORT (1 << EXCB_DATA_ABORT)
#define EXCB_INST_ABORT 1
#define EXCF_INST_ABORT (1 << EXCB_INST_ABORT)
#define EXCB_BREAKPOINT 2
#define EXCF_BREAKPOINT (1 << EXCB_BREAKPOINT)

/*
 * Interrupt System (Amiga-style)
 */
struct Interrupt {
    struct Node is_Node;
    void      (*is_Code)(void);
    void       *is_Data;
};

// Node types (ln_Type)
#define NT_INTERRUPT 2
#define NT_DEVICE    3
#define NT_LIBRARY   4
#define NT_MSGPORT   5
#define NT_MESSAGE   6
#define NT_TASK      1
#define NT_SIGNALSEMAPHORE 11

// Task Flags (tc_Flags)
#define TCF_UMODE   (1 << 0) // Task runs in User Mode (U-Mode)

// Task address-space flags (tc_AddressSpaceFlags)
#define TASF_USER_SPACE_READY (1 << 0)

// Task resource flags (tc_ResourceFlags)
#define TRF_FREE_STACK       (1 << 0)
#define TRF_FREE_TASK_STRUCT (1 << 1)
#define TRF_RELEASE_USER_IMAGE (1 << 2)
#define TRF_DESTROY_TASK_SPACE (1 << 3)

// Task exit reasons
#define TASK_EXIT_NONE       0
#define TASK_EXIT_RETURN     1
#define TASK_EXIT_SYS_EXIT   2
#define TASK_EXIT_EXCEPTION  3
#define TASK_EXIT_REMOVED    4

// InitSupervisor shell spawn status
#define SHELL_SPAWN_OK                 0
#define SHELL_SPAWN_FAIL_NO_DOS        1
#define SHELL_SPAWN_FAIL_LOADSEG       2
#define SHELL_SPAWN_FAIL_ALLOC         3
#define SHELL_SPAWN_FAIL_PREPARE_STACK 4

#include <exec/exec_lib.h>

// Funzioni per le liste
void NewList(struct List *list);
void AddHead(struct List *list, struct Node *node);
void AddTail(struct List *list, struct Node *node);
void Insert(struct List *list, struct Node *node, struct Node *listNode);
void Enqueue(struct List *list, struct Node *node);
void MoveList(struct List *destination, struct List *source);
struct Node *RemHead(struct List *list);
struct Node *RemTail(struct List *list);
struct Node *FindName(struct List *list, const char *name);
struct Node *GetHead(struct List *list);
struct Node *GetTail(struct List *list);
struct Node *GetSucc(struct Node *node);
struct Node *GetPred(struct Node *node);
void Remove(struct Node *node);

// Messaging
void PutMsg(struct MsgPort *port, struct Message *msg);
struct Message* GetMsg(struct MsgPort *port);
void ReplyMsg(struct Message *msg);
struct Message* WaitPort(struct MsgPort *port);
struct MsgPort* CreateMsgPort(void);
void DeleteMsgPort(struct MsgPort *port);

// Semaphores
void InitSemaphore(struct SignalSemaphore *sigSem);
void ObtainSemaphore(struct SignalSemaphore *sigSem);
void ReleaseSemaphore(struct SignalSemaphore *sigSem);

// Tasks
void AddTask(struct Task *task, struct ExecCPU *targetCPU);
void RemTask(struct Task *task);
struct Task *FindTask(const char *name);
void SetExcept(void (*handler)(void *), uint32_t flags);

// Signals
void Signal(struct Task *task, uint32_t sigSet);
uint32_t SetSignal(uint32_t newSignals, uint32_t signalMask);
void Forbid(void);
void Permit(void);
uint32_t Wait(uint32_t sigSet);
int8_t AllocSignal(int8_t signalNum);
void FreeSignal(int8_t signalNum);

// Interrupts
void AddIntServer(uint32_t irq, struct Interrupt *is);
void RemIntServer(uint32_t irq, struct Interrupt *is);
void EnableIRQ(uint32_t irq);
void DisableIRQ(uint32_t irq);

// Memoria
void*    AllocMem(uint32_t byteSize, uint32_t attributes);
void     FreeMem(void *memory, uint32_t byteSize);
uint32_t AvailMem(uint32_t attributes);
int MakeExecutable(void *ptr, uint32_t size);

// Libraries and interfaces
struct Library *OpenLibrary(const char *name, uint32_t version);
void CloseLibrary(struct Library *lib);
void RemLibrary(struct Library *lib);
struct Interface *GetInterface(struct Library *lib, const char *name, uint32_t version, void *taglist);
void AddLibrary(struct Library *lib);
void AddInterface(struct Library *lib, struct Interface *iface);

// Devices
struct Device *OpenDevice(const char *name, uint32_t version);
void CloseDevice(struct Device *dev);

// Utility e Arch
extern char _arch_uart_getc(void);
extern int strcmp(const char *s1, const char *s2);
void long_to_string(uint32_t n, char *s);
void shell_main(void);

#endif
