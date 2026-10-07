#ifndef EXEC_BASE_H
#define EXEC_BASE_H

#include <exec/exec.h>

struct IntuitionBase;
struct IntuitionInterface;

// Numero massimo di core supportati (JH7110/MilkV Mars = 4 hart, QEMU virt max = 8)
#define MAX_CPUS 8
#define CPUF_ALIVE 0x01u

/*
 * Struct ExecCPU
 * Rappresenta un singolo core fisico.
 * Allineata a 64 byte per evitare il "cache trashing" tra core vicini.
 */
struct ExecCPU {
    struct List  cpu_ReadyList;      // La lista privata dei Task pronti per QUESTO core
    struct Task *cpu_CurrentTask;    // Il Task che sta girando ORA su questo core
    struct Task *cpu_IdleTask;       // Il Task "fannullone" (gira quando non c'è altro da fare)
    
    uint32_t     cpu_ID;             // ID del Core (0, 1, 2...)
    uint32_t     cpu_Flags;          // Stato del processore
    uint64_t     cpu_SwitchCount;    // Statistiche: quanti context switch fatti
    
    spinlock_t   cpu_Lock;           // Lock per proteggere ReadyList
    
} __attribute__((aligned(64)));


struct VirtIODeviceInfo {
    uint32_t device_id;
    uintptr_t base;
    uint32_t irq;
};

#define MAX_VIRTIO_DEVICES 32

#define EXEC64_PROFILE_PROTECT      0
#define EXEC64_PROFILE_PERFORMANCE  1

#define EXEC64_LOADMODE_SUPERVISOR  0
#define EXEC64_LOADMODE_USER        1

/*
 * Struct ExecBase
 * La struttura globale di Exec64.
 * Contiene i processori, ma anche le risorse condivise da tutto il sistema.
 */
struct ExecBase {
    // Array delle CPU fisiche
    struct ExecCPU ex_CPUs[MAX_CPUS];
    
    // Numero di CPU attive
    uint32_t       ex_NumCPUs;
    
    // --- RISORSE GLOBALI DI SISTEMA ---
    
    // Lista GLOBALE di tutti i task esistenti (utile per comandi tipo "ps" o "avail")
    struct List    ex_TaskGlobalList;
    
    spinlock_t     ex_TaskLock; // Lock per la lista globale dei task (SMP Safe)
    
    // Lista delle librerie caricate (exec.library, dos.library, graphics.library...)
    struct List    ex_LibraryList;
    
    // Lista dei device (timer.device, serial.device...)
    struct List    ex_DeviceList;
    
    // Lista delle porte messaggi pubbliche (per la comunicazione tra programmi)
    struct List    ex_PortList;
    
    // Sostituito ex_MemoryHeader con la lista per la RAM
    struct List    ex_MemList; 

    // Lista per la gestione efficiente dei timer (Tasks in attesa ordinati per tick)
    struct List    ex_TimerList;

    // Funzione di debug (impostata dall'architettura)
    void (*ex_DebugPutS)(const char *s);
    void (*ex_DebugPutC)(char c);
    char (*ex_DebugGetC)(void);

    // --- NUOVE STRUTTURE SDK ---
    struct Library        ex_ExecLib;    // La struttura base della exec.library
    struct ExecInterface *ex_IExec;      // Il puntatore all'interfaccia principale (IExec)
    
    struct IntuitionBase *ex_IntuitionBase; // [NEW] Pointer to intuition.library base
    struct IntuitionInterface *ex_IIntuition; // [NEW] IIntuition interface
    void                *ex_IImPulse;
    struct Interface    *ex_IDos;        // [NEW] DOS interface pointer
    uint32_t             ex_SystemProfile; // Boot profile policy for session loaders
    uint32_t             ex_ActiveLoadMode; // Current loader mode while initializing a module/library

    struct Console       *ex_Console;    // Puntatore alla console grafica (Fase 4)

    // Minimal shell supervision state
    struct Task         *ex_InitTask;
    struct Task         *ex_ShellTask;
    uint32_t             ex_ShellSupervisorSig;
    uint32_t             ex_ShellGeneration;
    uint32_t             ex_ShellRestartCount;
    uint32_t             ex_ShellSpawnRetryCount;
    uint32_t             ex_LastShellExitReason;
    uint32_t             ex_LastShellSpawnStatus;
    uint64_t             ex_LastShellExitDetail;

    // VirtIO Device Registry for Modular Drivers
    struct VirtIODeviceInfo ex_VirtIODeviceList[MAX_VIRTIO_DEVICES];
    uint32_t                ex_VirtIODeviceCount;

    uintptr_t               ex_PlatformData; // [NEW] Store DTB pointer or platform metadata
};

// Variabile globale del sistema
extern struct ExecBase SysBase;

// Funzione inline per ottenere il Core ID corrente
static inline uint32_t get_current_core_id(void) {
#if defined(ARCH_ARM64)
    uint64_t id;
    asm volatile("mrs %0, mpidr_el1" : "=r" (id));
    return (uint32_t)(id & 0xFF);
#else
    uint64_t id;
    asm volatile("mv %0, tp" : "=r" (id));
    return (uint32_t)id;
#endif
}

// Macro per ottenere la CPU corrente
#define THIS_CPU (&SysBase.ex_CPUs[get_current_core_id()])

// Funzioni Kernel Helper
void *PrepareStack(struct Task *task, void (*code)(void), void *arg);

#endif
