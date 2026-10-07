# Exec64: Contratto Tecnico e Interfacce API (Immutabile)

**Versione**: 1.2 (RISC-V)
**Data**: 24 Agosto 2026

---

## 1. Il Kernel: System Call Interface (`ecall`)

I numeri di syscall sono fissi. Il registro `a7` contiene il numero della syscall, `a0-a5` gli argomenti. La sola tabella autorevole e `sdk/include/exec/syscalls.h`; questa sezione e un riepilogo documentale.

| Numero | Nome | Descrizione | Prototipo |
| :--- | :--- | :--- | :--- |
| **0** | `SYS_YIELD` | Rilascia il controllo alla CPU | `void Sys_Yield()` |
| **1** | `SYS_KPUTS` | Stampa una stringa debug | `void Sys_KPutS(char*)` |
| **2** | `SYS_KGETC` | Legge un carattere dalla console | `int Sys_KGetC()` |
| **3** | `SYS_ALLOC_MEM` | Alloca memoria heap | `void* Sys_AllocMem(size, flags)` |
| **4** | `SYS_FREE_MEM` | Libera memoria heap | `void Sys_FreeMem(ptr)` |
| **5** | `SYS_GET_TICK` | Ottiene il timestamp di sistema | `uint64_t Sys_GetTick()` |
| **6-18** | `SYS_DOS_*` | Primitive DOS iniziali | - |
| **19** | `SYS_DOS_UNLOADSEG` | Scarica un segmento ELF | `UnLoadSeg(segment)` |
| **20** | `SYS_EXIT` | Termina il processo corrente | `void Sys_Exit(status)` |
| **21-23** | memoria/controllo/DOS | `AvailMem`, `Halt`, `MakeDir` | - |
| **24-27** | tasking | Creazione, stack, rimozione e task corrente | - |
| **28-33** | IPC | Messaggi e porte | - |
| **34-44** | runtime/moduli/MMIO | W^X, eccezioni, device, librerie e diagnostica MMIO | - |
| **45** | `SYS_DOS_GETVOLUMEINFO` | Statistiche del volume `SYS:` | - |
| **46** | `SYS_DMA_SYNC` | Sincronizzazione DMA controllata | - |
| **47-49** | Boot Resource | Capability block info/read/write | - |
| **50-51** | Image loader | Load/unload ELF da buffer U-Mode | - |
| **52** | `SYS_DO_IO` | Richiesta sincrona validata verso un device aperto | `DoIO(request)` |
| **53** | `SYS_DELAY_MS` | Attesa temporizzata e schedulabile del task corrente | `DelayMs(ms)` |

---

## 2. DOS Library: Filesystem Syscalls

Le syscall DOS elencate qui sono il contratto legacy ancora disponibile durante
la migrazione. Il modello finale conserva in Core solo primitive generiche;
`dos.library` ed `exfat.handler` appartengono a Exec64 OS e usano capability
opache. I numeri esistenti non vengono comunque rinumerati.

| Numero | Nome | Descrizione |
| :--- | :--- | :--- |
| **6** | `SYS_DOS_OPEN` | Apre un file o directory |
| **7** | `SYS_DOS_CLOSE` | Chiude un file handle |
| **11** | `SYS_DOS_RESOLVE` | Risolve un percorso Amiga (`SYS:`, `/`) |
| **12** | `SYS_DOS_LOADSEG` | Carica un binario ELF in memoria |
| **17** | `SYS_DOS_CD` | Cambia la Current Working Directory |
| **45** | `SYS_DOS_GETVOLUMEINFO` | Restituisce statistiche totali/usate/libere del volume `SYS:` |

---

## 3. Librerie Dinamiche: L'Ordine delle Funzioni (Interface V-Table)

Per le librerie esterne (`graphics.library`, `intuition.library`), il "contratto" è l'**ordine dei puntatori a funzione** nella struttura Interface. Se l'ordine cambia, un'app compilata prima smetterà di funzionare.

### 🎨 Graphics Library (`GraphicsIFace`) - Ordine Fisso:
1. `InitRastPort`
2. `WritePixel`
3. `ReadPixel`
4. `RectFill`
5. `DrawLine`
6. `DrawRect`
7. `DrawChar`
8. `DrawText`
...ecc.

### 🖼️ Intuition Library (`IntuitionInterface`) - Ordine Fisso:
1. `OpenScreenTagList`
2. `CloseScreen`
3. `OpenWindowTagList`
4. `CloseWindow`
5. `ActivateWindow`
6. `ModifyIDCMP`
7. `DisplayAlert`
8. `GetTagData`
9. `RenderWindow`
10. `HandleInput`

---

## 💾 DOS Library (`DOSInterface`) - Ordine Fisso:
*(Qualora non invocata via syscall diretta)*
1. `Open`
2. `Close`
3. `Read`
4. `Write`
5. `Delete`
6. `Rename`
7. `Lock`
8. `UnLock`

---

## 🛑 Regola d'Oro per l'Evoluzione
1. **Immutabilità**: È vietato rimuovere o cambiare l'ordine delle syscall o delle funzioni esistenti.
2. **Estensione**: È ammesso solo aggiungere nuove funzioni in CODA (append-only) nelle nuove versioni della libreria o nuovi numeri di syscall.
3. **Firmware Compatibility**: Un binario compilato con la versione 1.0 del contratto deve poter girare senza modifiche su un kernel versione 2.0 (Retrocompatibilità).
