# Exec64 Kernel API Reference (`IExec`) - v1.3

Questo documento descrive l'interfaccia **Exec Library (`IExec`)**, lo standard ufficiale per interagire con il kernel Exec64.

## Metodo di Accesso
Il puntatore globale `IExec` deve essere utilizzato per ogni chiamata di sistema.

```c
#include "exec_lib.h"

void Main() {
    IExec->DebugPutS("Exec64 System Ready.\n");
}
```

---

## 1. Gestione Memoria (Memory.c)

### `AllocVec` / `FreeVec`
Utility per allocazioni con tracciamento della dimensione (header di 16 byte).
- **Prototipo**: `void *AllocVec(uint32_t byteSize, uint32_t attributes)`
- **Prototipo**: `void FreeVec(void *ptr)`
- **Note**: `AllocVec` memorizza la dimensione totale all'indirizzo `ptr - 16`. **CAUTION**: Attualmente non valida i puntatori in input.

### `AllocMem` / `FreeMem` / `AvailMem`
Funzioni core dell'allocatore (allineamento 64-byte).
- `void *AllocMem(uint32_t byteSize, uint32_t attributes)`
- `void FreeMem(void *ptr, uint32_t byteSize)`
- `uint32_t AvailMem(uint32_t attributes)`

### `CreatePool` / `DeletePool` (Memory Pools)
Gestione efficiente della memoria per allocazioni frequenti (riduce frammentazione e lock contention).
- **Prototipo**: `void *CreatePool(uint32_t flags, uint32_t puddleSize, uint32_t threshSize)`
    - `flags`: Attributi della memoria (es. `MEMF_CLEAR`).
    - `puddleSize`: Dimensione dei blocchi (Puddles) richiesti al sistema (es. 4096).
    - `threshSize`: Dimensione oltre la quale l'allocazione diventa dedicata (es. 2048).
- **Prototipo**: `void DeletePool(void *poolHeader)`
    - Libera *tutta* la memoria associata al pool in un colpo solo.

### `AllocPooled` / `FreePooled`
- **Prototipo**: `void *AllocPooled(void *poolHeader, uint32_t size)`
- **Prototipo**: `void FreePooled(void *poolHeader, void *memory, uint32_t size)`
    - **Nota**: In questa versione, `FreePooled` è una no-op veloce. La memoria viene recuperata solo con `DeletePool`.

### `ValidatePtr` (Security)
Verifica se un puntatore e la relativa dimensione ricadono in una regione di memoria RAM valida e allocata.
- **Prototipo**: `int ValidatePtr(const void *ptr, uint32_t size)`
- **Ritorna**: `1` se l'area è valida, `0` se l'indirizzo è nullo, fuori dai bound o non mappato.
- **Uso**: Fondamentale per i driver prima di eseguire DMA o accessi diretti a buffer forniti dai task.

---

## 1.1 Gestione Liste (Kernel.c / Utility)

### `NewList` / `AddHead` / `AddTail` / `Remove` / `RemHead`
Funzioni fondamentali per la gestione delle liste doppiamente concatenate in stile Exec.
- `void NewList(struct List *list)`: Inizializza una lista vuota.
- `void AddHead(struct List *list, struct Node *node)`: Inserisce un nodo in testa alla lista.
- `void AddTail(struct List *list, struct Node *node)`: Inserisce un nodo in coda alla lista.
- `void Remove(struct Node *node)`: Rimuove un nodo specifico dalla lista in cui si trova.
- `struct Node *RemHead(struct List *list)`: Rimuove e ritorna il primo nodo della lista.

---

## 2. Task Management (Kernel.c / Context_switch.S)

### `AddTask`
Rende un task "pronto" e lo assegna a una CPU fisica.
- **Prototipo**: `void AddTask(struct Task *task, struct ExecCPU *targetCPU)`
- **Arguments**:
    - `task`: Struttura Task già inizializzata e con stack preparato (tramite `PrepareStack`).
    - `targetCPU`: Puntatore alla CPU specifica (`SysBase.ex_CPUs[i]`) o `NULL` per assegnarlo al core corrente.
- **Note**: ABI aggiornata per ARM64. Nelle versioni precedenti il task veniva creato internamente, ora è responsabilità del chiamante.

### `RemTask`
Rimuove definitivamente un task dal sistema.
- **Prototipo**: `void RemTask(struct Task *task)`

### `AddSupervisedTask` / `TakeTaskExit`
Avviano un task U-Mode gia preparato e consegnano al supervisore un evento di
terminazione bounded, senza esporre strutture private del Core.

- `int32_t AddSupervisedTask(struct Task *task, struct Exec64TaskWatch *watch)`
- `int32_t TakeTaskExit(Exec64TaskHandle handle, struct Exec64TaskExitInfo *info)`

Il chiamante attende `watch.etw_SignalMask` tramite `Wait()` e usa poi
`watch.etw_Handle` per consumare l'evento. I metodi sono append-only in coda a
`ExecInterface`; il contratto completo e in
[../design/exec64_task_supervision_abi.md](../design/exec64_task_supervision_abi.md).

### `SetExcept` (Exception Handling - Phase 3)
Installa un handler di eccezioni specifico per il task corrente.
- **Prototipo**: `void SetExcept(void (*handler)(void *), uint32_t flags)`
- **Arguments**:
    - `handler`: Puntatore alla funzione da eseguire in caso di eccezione. Riceve come argomento un puntatore alla struct `ExceptionContext`.
    - `flags`: Maschera dei bit di eccezione da catturare (es. `EXCF_DATA_ABORT`).
- **Note**: L'handler viene eseguito nel contesto (stack/privilegi) del task. Se l'eccezione non è coperta dai flags, scatta il Guru Meditation globale.

---

## 3. Inter-Process Communication (Messaging.c)

### `CreateMsgPort` / `DeleteMsgPort`
- `struct MsgPort *CreateMsgPort()`
- `void DeleteMsgPort(struct MsgPort *port)`

### `PutMsg` / `GetMsg` / `ReplyMsg`
Invia e riceve messaggi asincroni (zero-copy).
- `void PutMsg(struct MsgPort *port, struct Message *msg)`
- `struct Message *GetMsg(struct MsgPort *port)`
- `void ReplyMsg(struct Message *msg)`

### `WaitPort`
Blocca il task finché non arriva un messaggio sulla porta.
- `struct Message *WaitPort(struct MsgPort *port)`

---

## 4. Segnali e Sincronizzazione (Messaging.c)

### `Wait`
Sospende il task finché non riceve uno o più segnali specifici.
- **Prototipo**: `uint32_t Wait(uint32_t signalSet)`
- **Note**: Implementato con istruzione `wfi` (Wait for Interrupt) su RISC-V per massimizzare il risparmio energetico dei core in idle.

### `Signal`
Invia segnali a un task specifico.
- **Prototipo**: `void Signal(struct Task *task, uint32_t signals)`

### `AllocSignal` / `FreeSignal`
Gestione dinamica dei bit di segnale (0-31).
- `int8_t AllocSignal(int8_t signalNum)` (Passa -1 per cercare il primo libero)
- `void FreeSignal(int8_t signalNum)`

### 4.1 Signal Semaphores (Messaging.c)
I semafori forniscono un meccanismo di mutua esclusiva task-safe. A differenza degli spinlock, se un semaforo è occupato, il task chiamante viene sospeso (`Wait`) finché la risorsa non torna libera.

**Struttura**:
```c
struct SignalSemaphore {
    struct Node     ss_Link;
    struct Task    *ss_Owner;
    int32_t         ss_NestCount;
    struct List     ss_WaitQueue;
    spinlock_t      ss_Lock; 
};
```

**Funzioni**:
- `void InitSemaphore(struct SignalSemaphore *sigSem)`: Inizializza la struttura del semaforo.
- `void ObtainSemaphore(struct SignalSemaphore *sigSem)`: Tenta di acquisire il lock esclusivo.
    - Se libero: il task diventa owner (`NestCount = 1`).
    - Se già owner: incrementa `NestCount` (Lock ricorsivo).
    - Se occupato: il task viene aggiunto alla `WaitQueue` e sospeso finché non riceve `SIGF_SEMAPHORE`.
- `void ReleaseSemaphore(struct SignalSemaphore *sigSem)`: Rilascia il lock.
    - Decrementa `NestCount`.
    - Se arriva a 0: passa la proprietà al prossimo task in coda e lo sveglia (`Signal`).

---

## 5. Sistemi di Tempo (Time.c - `IExec`)

Il kernel gestisce il tempo tramite il Timer IRQ con una granularità di 10ms.

| Funzione | Prototipo | Descrizione |
|----------|-----------|-------------|
| `GetTicks` | `uint64_t GetTicks(void)` | Ticks totali dal boot (1 tick = 10ms). |
| `GetTicksMs` | `uint32_t GetTicksMs(void)` | Tempo di uptime in millisecondi. |
| `DelayMs` | `void DelayMs(uint32_t ms)` | Sospende il task per N millisecondi. |

---

## 6. Gestione Librerie (Exec_library.c)

### `OpenLibrary` / `CloseLibrary` / `GetInterface`
- `struct Library *OpenLibrary(const char *name, uint32_t version)`
- `void CloseLibrary(struct Library *lib)`
- `struct Interface *GetInterface(struct Library *lib, const char *name, uint32_t version, void *taglist)`

---

## 7. Interrupt & Hardware
- `void AddIntServer(uint32_t irq, struct Interrupt *is)`
- `void EnableIRQ(uint32_t irq)`
- `void DebugPutS(const char *s)` (Output sincrono su seriale)
- `void Halt()` (Spegne/Ferma il sistema)
