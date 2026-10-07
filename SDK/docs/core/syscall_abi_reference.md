# Exec64 RISC-V API & Syscall Reference

**Last Updated**: August 29, 2026
**Version**: 1.6 (RISC-V Native)

## 📡 System Call Interface (`ecall`)

In User Mode (U-Mode), le funzioni del kernel devono essere chiamate tramite l'istruzione `ecall`. Le funzioni sono identificate da un numero di syscall passato nel registro `a7`.

### Syscall ABI
- **Fonte autorevole**: `sdk/include/exec/syscalls.h`.
- **Registro `a7`**: Numero della funzione definito dall'header autorevole.
- **Registri `a0 - a5`**: Parametri della funzione.
- **Ritorno**: Il valore di ritorno è sempre in `a0`.

### Elenco System Call Principali

| Nome | Numero (a7) | Descrizione |
| :--- | :--- | :--- |
| **SYS_YIELD** | 0 | Cede volontariamente la CPU al prossimo task. |
| **SYS_KPUTS** | 1 | Stampa una stringa (sincrona) sulla seriale del kernel. |
| **SYS_KGETC** | 2 | Legge un carattere dalla console. |
| **SYS_ALLOC_MEM** | 3 | Alloca memoria RAM (ritorna puntatore utente). |
| **SYS_FREE_MEM** | 4 | Libera memoria RAM. |
| **SYS_GET_TICK** | 5 | Ottiene il timestamp di sistema. |
| **SYS_DOS_UNLOADSEG** | 19 | Scarica un segmento ELF precedentemente caricato. |
| **SYS_EXIT** | 20 | Termina il task corrente in modo pulito. |
| **...** | ... | ... |
| **SYS_DOS_*** | 6-19, 23, 45 | System Call dedicate al filesystem (DOS). |
| **SYS_DOS_GETVOLUMEINFO** | 45 | Copia in userland le statistiche del volume `SYS:` tramite `struct DosVolumeInfo`. |
| **SYS_DMA_SYNC** | 46 | Sincronizza un range DMA secondo il contratto Core. |
| **SYS_BOOT_RESOURCE_INFO** | 47 | Legge il descrittore pubblico della Boot Resource. |
| **SYS_BOOT_RESOURCE_READ** | 48 | Legge blocchi dalla Boot Resource in un buffer U-Mode. |
| **SYS_BOOT_RESOURCE_WRITE** | 49 | Scrive blocchi quando il backend lo consente. |
| **SYS_IMAGE_LOAD** | 50 | Carica un'immagine ELF fornita da U-Mode applicando W^X. |
| **SYS_IMAGE_UNLOAD** | 51 | Scarica un'immagine caricata con `SYS_IMAGE_LOAD`. |
| **SYS_DO_IO** | 52 | Esegue una richiesta sincrona validata su un device aperto. |
| **SYS_DELAY_MS** | 53 | Sospende il task corrente usando il timer del Core, senza busy polling. |
| **SYS_ADD_SUPERVISED_TASK** | 54 | Avvia un task U-Mode preparato e restituisce handle opaco e segnale di terminazione. |
| **SYS_TAKE_TASK_EXIT** | 55 | Consuma l'evento bounded di un task supervisionato. |
| **SYS_WAIT_SIGNAL** | 56 | Attende una signal mask allocata al task U-Mode chiamante. |

---

## 📂 Amiga-Style Path Navigation

La `dos.library` su RISC-V implementa una logica di navigazione dei percorsi coerente con lo standard AmigaOS, ma operando sotto protezione hardware.

### Token Speciali
- `/`: Risale alla directory **parente** (es. `CD /`, `CD //`).
- `:`: Ritorna alla **radice del volume** corrente (es. `CD :`).
- `Volume:Path`: Risoluzione assoluta tramite volume name (es. `SYS:C/`).

### Implementazione Interna
1.  **Resolved Path Storage**: Il kernel mantiene il percorso assoluto come stringa (es. `SYS:Developers/Source`).
2.  **Parent Resolution**: Quando viene ricevuto `/`, il resolver cerca l'ultimo separatore `:` o `/` per troncare la stringa e risalire il livello.
3.  **Volume Preservation**: Se il percorso inizia con `:`, il sistema estrae il nome del volume correntemente montato per garantire che il prompt rimanga accurato (`SYS: >`).

---

## 🛠️ SDK & Trampolines

Per semplificare lo sviluppo, il SDK fornisce dei "trampolini" in C che nascondono l'uso dell'assembly `ecall`.

**File**: `sdk/libc/umode_trampolines.c`

Il trampoline di terminazione carica `SYS_EXIT` (`20`) in `a7` prima di
eseguire `ecall`. Il kernel usa la stessa definizione nel dispatcher; nessun
consumer deve mantenere una copia privata della numerazione.

Le syscall Boot Resource accettano soltanto handle opachi owner-bound. Il
contratto completo, inclusi allineamento, size massima e lifetime, e in
[../design/exec64_boot_resource_abi.md](../design/exec64_boot_resource_abi.md).

`SYS_DO_IO` non espone MMIO o callback arbitrari a U-Mode. Il Core verifica il
puntatore alla richiesta, la sua dimensione, il device gia aperto e l'eventuale
buffer dati, quindi richiama il metodo `dd_DoIO` del device nel page table
kernel. Il primo consumer e il present a grana grossa di `virtiogpu.device`.

`SYS_DELAY_MS` riceve in `a0` un intervallo in millisecondi e usa la coda timer
del Core. La syscall sostituisce le precedenti attese U-Mode simulate con una
sequenza di `SYS_YIELD`, che non forniva alcuna garanzia temporale.

Le syscall di supervisione usano le strutture versionate definite in
`core/include/uapi/exec64/task.h`. Il Core non espone slot o puntatori interni:
l'handle e owner-bound, il segnale e allocato al supervisore e l'evento viene
invalidato dopo il consumo. Il contratto completo e in
[../design/exec64_task_supervision_abi.md](../design/exec64_task_supervision_abi.md).
