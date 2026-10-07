#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <exec/exec.h>

// --- MACRO E FLAG DI MEMORIA (Amiga Style) ---
#define MEMF_ANY    0
#define MEMF_PUBLIC (1 << 0)
#define MEMF_CHIP   (1 << 1)
#define MEMF_FAST   (1 << 2)
#define MEMF_EXECUTABLE (1 << 3) // Richiede memoria con permessi di esecuzione
#define MEMF_SHARED     (1 << 4) // Memoria coerente per periferiche (I/O)
#define MEMF_ALIGNED    (1 << 5) // Allineamento a 4KB (pagina)
#define MEMF_CLEAR      (1 << 16) // Azzera la memoria dopo l'allocazione

// --- STRUTTURE DATI ---

/*
 * MemChunk: Rappresenta un blocco di memoria libera.
 * Si trova all'inizio di ogni zona di RAM non utilizzata.
 */
struct MemChunk {
    struct MemChunk *mc_Next;  // Puntatore al prossimo blocco libero
    uint32_t         mc_Bytes; // Dimensione di questo blocco
};

/*
 * MemHeader: Descrive un'intera regione di memoria (es. Main RAM).
 */
struct MemHeader {
    struct Node      mh_Node;       // Permette di inserirlo nella ex_MemList
    uint16_t         mh_Attributes; // Flag (MEMF_PUBLIC, etc.)
    void            *mh_First;      // Puntatore al primo MemChunk libero
    void            *mh_Lower;      // Limite inferiore della regione
    void            *mh_Upper;      // Limite superiore della regione
    uint32_t         mh_Free;       // Totale byte liberi in questa regione
};

// --- PROTOTIPI DELLE FUNZIONI ---

/**
 * Inizializza il pool di memoria principale.
 */
void InitMemory(struct MemHeader *mh, void *start, uint32_t size, uint32_t attributes, const char *name);

/**
 * Alloca un blocco di memoria.
 */
void *AllocMem(uint32_t byteSize, uint32_t attributes);

/**
 * Libera un blocco di memoria precedentemente allocato.
 */
void FreeMem(void *memory, uint32_t byteSize);

/**
 * Alloca un blocco di memoria e ne memorizza la dimensione internamente.
 */
void *AllocVec(uint32_t byteSize, uint32_t attributes);

/**
 * Libera un blocco allocato con AllocVec.
 */
void FreeVec(void *memory);

/**
 * Restituisce la quantità totale di memoria libera nel sistema.
 */
/**
 * Restituisce la quantità totale di memoria libera nel sistema.
 */
uint32_t AvailMem(uint32_t attributes);

// --- MEMORY POOLS ---

/*
 * MemPool: Struttura per la gestione efficiente della memoria.
 * Evita la frammentazione allocando grandi blocchi (Puddles) e suddividendoli.
 */
struct MemPool {
    struct Node     mp_Node;       // Per liste di pool (opzionale)
    uint32_t        mp_Flags;      // Flag del pool (MEMF_CLEAR, etc.)
    uint32_t        mp_PuddleSize; // Dimensione di ogni "pozzanghera" (es. 4KB)
    uint32_t        mp_ThreshSize; // Allocazioni > di questo vanno dirette a AllocMem
    struct List     mp_PuddleList; // Lista dei blocchi di memoria (Puddles)
    spinlock_t      mp_Lock;       // Lock per proteggere il pool
};

/*
 * Puddle: Un grande blocco di memoria gestito dal pool.
 * Header interno invisibile all'utente.
 */
struct Puddle {
    struct Node     pd_Node;       // Link nella mp_PuddleList
    uint32_t        pd_Size;       // Dimensione totale del Puddle
    uint32_t        pd_Free;       // Byte liberi rimasti
    void           *pd_NextAlloc;  // Puntatore al prossimo byte libero
    // La memoria utile segue qui...
};

// Funzioni Pool
void *CreatePool(uint32_t flags, uint32_t puddleSize, uint32_t threshSize);
void DeletePool(void *poolHeader);
void *AllocPooled(void *poolHeader, uint32_t size);
void FreePooled(void *poolHeader, void *memory, uint32_t size);

#endif
