# EXEC64 — Kernel Foundation Document
**Versione**: 1.2
**Data**: 1 ottobre 2026
**Stato**: DOCUMENTO NORMATIVO — le regole qui definite sono vincolanti per tutto il codice del progetto.

---

## 0. Missione del Progetto

Exec64 è un kernel professionale a 64-bit per architettura RISC-V, ispirato alla filosofia di AmigaOS (strutture dati basate su nodi, messaggistica asincrona, librerie a interfaccia stabile) e progettato per sfruttare le caratteristiche native di RISC-V (S-Mode, SBI, SV39, PMP, PLIC).

**Target primario di sviluppo**: QEMU/KVM con VirtIO (piattaforma di sviluppo principale).
**Target bare-metal supportato**: MilkV Mars (StarFive JH7110, RISC-V RV64GC, 4 hart).
**Modello di deployment**: Il kernel viene caricato da U-Boot come immagine Linux-compatibile grazie all'header standard incluso in `boot.S`.

Il kernel non è e non sarà mai "un altro hobby OS". È un progetto serio con obiettivi chiari:
- Correttezza formale del path critico (boot, context switch, trap, MMU).
- Sicurezza strutturale non opzionale (W^X, validazione puntatori user, isolamento ring).
- SMP come first-class citizen, non un'aggiunta tardiva.
- ABI stabile per le librerie, indipendente dalle versioni del kernel.
- Ecosistema **dual-mode** con profili Protect, Performance e Mixed governati
  da policy esplicite e ABI comuni.

---

## 1. Principi Architetturali Immutabili

Questi principi non possono essere violati in nessuna circostanza. Ogni PR, patch o modifica che li viola deve essere rifiutata.

### 1.1 Separazione dei Privilegi e Profili di Sistema (S-Mode / U-Mode)

Il kernel base opera **esclusivamente in S-Mode** (Supervisor Mode).
Il profilo predefinito conservativo del sistema e **protect mode**:

- applicazioni e comandi ordinari vivono in `U-Mode`
- i confini kernel/user restano rigorosi
- nessun componente non autorizzato accede direttamente a CSR, MMIO o strutture kernel
- la transizione `S-Mode ↔ U-Mode` avviene solo tramite `ecall` (syscall) o eccezione hardware

Questo resta il modello di riferimento per sicurezza, resilienza e fault containment.

Decisione del maintainer, 2026-10-03: il target **Protect puro** lascia in
S-Mode soltanto il Core; servizi OS, device, librerie e applicazioni operano
in U-Mode. Il Core conserva i meccanismi privilegiati minimi per IRQ, MMIO,
DMA e IPC. La sola collocazione in U-Mode non certifica ancora isolamento
completo: driver USER e relativi contratti restano lavoro futuro.
La configurazione attuale (Core/device supervisor, librerie applicative e
applicazioni USER, con servizi residenti ancora nel Core) e il riferimento
da conservare per un futuro preset Mixed. Il nome runtime `protect` resta
transizionale; questa decisione non cambia oggi loader o preferenze.

Exec64, pero, non e pensato come microkernel rigido incapace di cambiare profilo operativo.
Il sistema supporta anche un **performance mode** che conserva il cuore operativo
Amiga-style: Core, servizi OS, librerie, device e applicazioni vengono eseguiti
in `S-Mode`, nello stesso dominio privilegiato e nello stesso spazio condiviso.
I componenti ammessi in questo profilo devono essere sviluppati, verificati e
considerati parte della Trusted Computing Base.

Questo non cambia la regola fondamentale:

- il livello di privilegio e una **policy deliberata del sistema**
- non una scorciatoia per aggirare sicurezza, ownership o contratti ABI

In altre parole:

- `protect mode` e il profilo moderno con isolamento della memoria
- `performance mode` e il profilo flat ad alte prestazioni: l'intero sistema
  operativo e le applicazioni sono in `S-Mode`
- `mixed mode` sceglie esplicitamente il dominio per ogni componente e applicazione
- nessun componente cambia dominio in modo implicito o disordinato
- la scelta deve essere governata da policy, stabilita del componente e contratti chiari

### 1.1B Dual-Mode come Direzione Identitaria del Progetto

Una delle caratteristiche piu originali di Exec64 e la possibilita, nel tempo, di supportare componenti capaci di vivere in piu profili di esecuzione.

Questo vale soprattutto per:

- driver
- librerie
- applicazioni selezionate

La direzione corretta e:

- mantenere una identita logica stabile del componente
- separare il piu possibile il core logico dal binding del privilege level
- permettere i tre profili `protect`, `performance` e `mixed`
- risolvere dominio e binding durante il caricamento o l'apertura, non durante
  ogni chiamata della funzione
- garantire che due componenti nello stesso dominio comunichino direttamente
- vietare che il supporto dual-mode introduca proxy, syscall, packet, copie,
  lookup di catalogo o controlli per chiamata nel path `S-Mode -> S-Mode` di
  `performance mode`

Exec64 non vuole scegliere una volta per tutte tra:

- isolamento massimo
- prestazioni massime

Vuole invece offrire tutti e tre i profili in modo disciplinato:

- `protect mode` come profilo moderno e difensivo
- `performance mode` come profilo flat Amiga-style per l'intero sistema trusted
- `mixed mode` come composizione esplicita dei due modelli

Questa direzione resta comunque subordinata ai principi immutabili della Foundation:

- W^X
- ABI stabile
- ownership esplicita e validazione rigorosa quando esiste un confine tra domini

### 1.1C Contratto Normativo dei Tre Profili

| Profilo | Core | OS, librerie e device | Applicazioni | Chiamate nello stesso dominio |
|---|---|---|---|---|
| `protect` | S-Mode | U-Mode per impostazione; piccoli binding privilegiati solo dove hardware o policy lo richiedono | U-Mode | Dirette U->U; bridge controllato soltanto quando si attraversa U/S |
| `performance` | S-Mode | S-Mode | S-Mode | Dirette S->S, senza overhead dual-mode |
| `mixed` | S-Mode | Scelta esplicita per componente | Scelta esplicita per applicazione | Dirette nello stesso dominio; bridge solo tra domini diversi |

`performance` non e una variante di Protect con piu moduli privilegiati. E il
profilo flat completo di Exec64. Le applicazioni ricevono le vere interfacce dei
moduli `S-Mode` e le invocano come normali funzioni C. Il catalogo e il loader
possono selezionare e validare il binding durante `OpenLibrary`, `OpenDevice` o
il lancio dell'applicazione, ma devono uscire dal percorso delle chiamate dopo
la risoluzione.

Il percorso `S->S` non usa facade USER, handle di transizione, marshalling o
controlli di ownership per ogni funzione. Reference count, caricamento,
scaricamento e diagnostica restano ammessi fuori dal fast path. Scheduler,
interrupt e servizi intenzionalmente asincroni conservano la loro semantica:
non sono overhead introdotto dal dual-mode.

Il costo della scelta dual-mode e ammesso soltanto quando una configurazione
`protect` o `mixed` attraversa realmente un confine di privilegio. Anche in quel
caso l'ABI pubblica dell'applicazione resta invariata.

### 1.2 W^X — Write XOR Execute (Non negoziabile)

Nessuna pagina di memoria può avere i permessi Write (W) e Execute (X) attivi simultaneamente.

| Segmento | R | W | X | U |
|---|---|---|---|---|
| Kernel text | ✓ | ✗ | ✓ | ✗ |
| Kernel rodata | ✓ | ✗ | ✗ | ✗ |
| Kernel data/bss/heap | ✓ | ✓ | ✗ | ✗ |
| User code (dopo loading) | ✓ | ✗ | ✓ | ✓ |
| User data/stack | ✓ | ✓ | ✗ | ✓ |
| Code heap (caricamento ELF) | ✓ | ✓ | ✗ | ✓ → poi ✗ |

Il caricamento degli ELF avviene in due fasi: (1) scrittura in pagine W senza X, (2) flip atomico a R-X prima di cedere il controllo al task.

### 1.3 IPC tramite Message Passing (Stile Amiga)

Il paradigma IPC pubblico resta quello Exec/Amiga:

- `MsgPort`
- `Message`
- `PutMsg()`
- `GetMsg()`
- `WaitPort()`
- `ReplyMsg()`

Protect e Performance condividono questa API e la relativa semantica
applicativa, ma non sono obbligati a usare lo stesso meccanismo interno ne lo
stesso livello di bookkeeping.

#### 1.3A Protect Mode

In `protect mode` il Core e l'autorita finale su porte, messaggi, mapping,
ownership e autorizzazioni.

- una `MsgPort *` ricevuta da `U-Mode` conserva il tipo sorgente Amiga-style,
  ma il suo valore e un token opaco validato mediante un registro autorevole
  del Core; non deve essere un indirizzo kernel esposto o dereferenziabile
- il Core verifica almeno i diritti di invio, ricezione e distruzione della
  porta
- i piccoli `Message` destinati all'IPC protetto vengono allocati con
  `AllocMessage()` e liberati con `FreeMessage()` da una arena IPC page-backed
- l'arena riserva una finestra VA dedicata per mantenere, dove possibile, lo
  stesso valore virtuale di `Message *` nei task partecipanti
- le pagine dell'arena sono mappate soltanto negli address space autorizzati;
  non sono globalmente visibili a tutti i task
- messaggi con insiemi di autorizzazioni incompatibili non condividono la
  stessa pagina fisica
- l'ownership segue almeno gli stati `sender -> queued -> receiver -> replied`;
  exit e crash non devono lasciare messaggi, mapping o metadati orfani

`PutMsg()` mantiene la firma Exec `void`. Una violazione del contratto IPC in
`U-Mode` protetto, per esempio token non valido, accesso non autorizzato,
messaggio non proveniente dall'arena, doppio invio o free durante un trasferimento,
produce diagnostica e un fault sincrono controllato del task responsabile. Non
deve mai corrompere le code o lo stato del Core. Non viene introdotta ora una
ABI pubblica `PutMsgChecked()`.

La `mn_Length` descrive la dimensione del `Message`; non descrive grandi
payload esterni.

#### 1.3B Payload grandi e page grant

I payload significativi, inclusi buffer audio, rete, storage, VirtIO e GPU,
usano page grant gestiti dal Core. Lo stesso frame fisico viene mappato negli
address space autorizzati senza copiare il payload soltanto per attraversare il
confine di protezione. L'indirizzo virtuale del payload non e obbligato a essere
uguale nei partecipanti.

Un grant possiede metadati Core almeno per owner, destinatari autorizzati,
accesso read-only/read-write, range di pagine, stato e lifecycle. Deve essere
associabile esplicitamente al messaggio che lo trasporta, cosi `ReplyMsg()`, la
revoca esplicita, l'exit o il crash possono chiuderlo secondo una policy
reply-scoped oppure persistent/reusable.

Il page grant IPC resta semanticamente distinto dal mapping DMA. Le primitive
MMU comuni possono essere riutilizzate, ma DMA richiede anche pinning,
coherency ed eventuale IOMMU.

#### 1.3C Performance Mode

In `performance mode` tutti i componenti partecipanti, comprese le applicazioni,
sono trusted e operano in `S-Mode`. Il percorso resta quello Exec originale:

`Message * -> queue -> Message *`

Questo fast path non esegue copie, remapping per singolo messaggio, proxy o
tracking Protect obbligatorio. Puntatori, code e interfacce sono direttamente
accessibili nel dominio condiviso. Un componente che deve restare isolato
richiede `protect` o una configurazione `mixed`, non un costo nascosto nel
profilo Performance.

#### 1.3D Regola generale

Exec64 conserva il paradigma Exec indipendentemente dal profilo di protezione.
Protect rende autorevoli Core, ownership, mapping e autorizzazioni; Performance
conserva il pointer passing diretto e il minimo overhead possibile.

Lo zero-copy non e un dogma per ogni ABI dell'OS. Piccoli record bounded di
Intuition, DOS o Desktop possono essere copiati quando e piu semplice, sicuro e
veloce. Lo zero-copy e invece un requisito del normale IPC nativo per i payload
di dimensioni significative.

### 1.4 Handle, Non Puntatori, tra Contesti Diversi

Le risorse kernel (porte, file, device) sono identificate da handle numerici opachi nei confronti del codice user.
Il kernel valida **sempre** i puntatori provenienti da U-Mode prima di dereferenziarli (tramite `is_user_ptr()`).

La firma pubblica `struct MsgPort *` costituisce una eccezione soltanto a
livello di tipo C per compatibilita sorgente con Exec. In `U-Mode` il valore e
un token opaco: il client non puo dereferenziarlo e deve ottenere le informazioni
necessarie, incluso il signal mask della porta, tramite API Core dedicate. In
`S-Mode` trusted la struttura puo invece essere direttamente accessibile secondo
il contratto Performance.

### 1.5 SMP come First-Class Citizen

Tutta la struttura dati mutabile del kernel è progettata per l'accesso concorrente da N hart.
Non esistono variabili globali mutabili senza protezione esplicita (spinlock, atomics, o garanzia di accesso single-CPU).
Le strutture per-CPU vivono in `ExecCPU` e sono accessibili tramite la macro `THIS_CPU` (che legge `tp`, il registro hartid).

### 1.6 ABI Stabile per le Librerie

Le interfacce delle librerie (IExec, IDos, IIntuition...) sono strutture di puntatori a funzione.
Una volta rilasciata una versione di libreria, i field esistenti non vengono mai spostati — si aggiungono solo nuovi field in coda.
Le applicazioni sono binary-compatible tra versioni diverse del kernel che implementano la stessa API.

### 1.7 Validazione dei Contratti al Boot

Ogni assunzione critica (allineamento struct, dimensioni, offsets) deve essere verificata con `_Static_assert` a compile time o con una funzione `arch_selftest()` invocata prima che il sistema diventi operativo.

---

## 2. Modello del Kernel

### 2.1 Cosa vive nel kernel (S-Mode)

| Componente | Posizione | Note |
|---|---|---|
| Boot / reset vector | `core/arch/riscv64/cpu/boot.S` | Entry point, BSS clear, SBI handoff |
| Trap / exception vector | `core/arch/riscv64/cpu/vectors.S` | SAVE_ALL / RESTORE_ALL, sret |
| Context switch | `core/arch/riscv64/cpu/context_switch.S` | Solo callee-saved per ABI RISC-V |
| IRQ / PLIC | `core/arch/riscv64/cpu/irq.c` | Timer, ext IRQ, yield (SSI) |
| Exception handler | `core/arch/riscv64/cpu/exception.c` | Guru Meditation, U-mode exit |
| Syscall handler | `core/arch/riscv64/cpu/irq.c` | `handle_syscall()` |
| MMU (SV39) | `core/arch/riscv64/cpu/mmu.c` | Walk, map, attivazione satp |
| Scheduler | `core/kernel/kernel.c` | Round-robin preemptivo prioritizzato |
| Memory allocator | `core/kernel/pool.c` + AllocMem | Amiga-style pool |
| IPC | `core/kernel/messaging.c` | MsgPort, Message, Signal |
| ExecBase | Globale | La radice del sistema |

### 2.2 Cosa vive fuori dal kernel (librerie/server)

| Componente | Ruolo | Profilo normale |
|---|---|---|
| dos.library | Servizi DOS, path e process management | `U-Mode` in Protect |
| Filesystem handler | Interpretazione del filesystem | `U-Mode` in Protect |
| graphics.library | Primitive grafiche | `U-Mode` in Protect |
| intuition.library | Windowing e gadget | `U-Mode` in Protect |
| impulse.library | Desktop e widget toolkit | `U-Mode` in Protect |
| Device/backend hardware | I/O specifico della piattaforma | `U-Mode` target in Protect; transizioni temporanee documentate in STATUS |
| Shell / console | Interazione utente | Task `U-Mode` |
| Applicazioni ELF | Programmi utente | Task `U-Mode` |

**Nota strategica**:
la tabella descrive il confine architetturale e il profilo Protect di destinazione,
non afferma che ogni migrazione sia gia completa. Lo stato transitorio reale e
documentato in `docs/STATUS/STATUS.md`.

In Performance, Core, componenti OS e applicazioni usano binding `S-Mode`
diretti e costituiscono insieme la Trusted Computing Base. Le librerie restano
moduli esterni al Core: condividere dominio e memoria non trasferisce la loro
logica nel kernel. Protect mantiene invece il confine moderno; Mixed decide per
singolo componente e usa un bridge soltanto quando i domini differiscono.

---

## 3. Contratto del Registro dei Processori (RISC-V)

### 3.1 Uso dei Registri Architetturali

| Registro | Nome ABI | Ruolo in Exec64 | Chi ne è responsabile |
|---|---|---|---|
| x0 | zero | Costante 0 | HW |
| x1 | ra | Return address | ABI / Context switch |
| x2 | sp | Stack pointer | Ogni task ha il suo sp |
| x3 | gp | Global pointer → `__global_pointer$` | **Impostato in boot.S, mai azzerato** |
| x4 | tp | Thread pointer → hartid corrente (S-Mode) / TLS task (U-Mode) | Boot.S lo imposta; **ripristinato in RESTORE_ALL** dal frame salvato (FIX-010) |
| x5-x7 | t0-t2 | Temporanei caller-saved | Non salvati in context_switch |
| x8 | s0/fp | Frame pointer (callee-saved) | Salvato in context_switch |
| x9-x27 | s1-s11 | Callee-saved | Salvati in context_switch |
| x28-x31 | t3-t6 | Temporanei caller-saved | Non salvati in context_switch |
| x10-x17 | a0-a7 | Argomenti / syscall | ABI; a7=syscall num in ecall |

### 3.2 Contratto del Context Switch

`_arch_switch_context(old_task, new_task)` salva e ripristina **solo** i registri callee-saved:

```
Saved: s0, s1-s11 (13 reg), ra, sp  → 14 × 8 = 112 bytes in struct Task
```

I registri caller-saved (t0-t6, a0-a7) **non** vengono salvati: è responsabilità del chiamante farlo prima di cedere la CPU. Questo è corretto per l'ABI RISC-V.

**Offset in struct Task (contratto vincolante)**:

| Campo | Offset | Dimensione | Note |
|---|---|---|---|
| `tc_StackPointer` (sp) | 48 | 8 | Verificare con offsetof() |
| `tc_Registers[0]` (s1) | 96 | 8 | Inizio array callee-saved |
| `tc_Registers[1-10]` (s2-s11) | 104-176 | 8 ciascuno | |
| `tc_Registers[11]` (ra) | 184 | 8 | Entry point per nuovi task |
| `tc_Registers[12]` (s0/fp) | 192 | 8 | |

**AZIONE RICHIESTA**: Aggiungere `_Static_assert(offsetof(struct Task, tc_StackPointer) == 48, ...)` e analoghi per tutti gli offset usati nel codice assembly.

### 3.3 Contratto FPU (Lazy Save/Restore)

La FPU viene gestita con strategia lazy:
1. Al context switch, `sstatus.FS` viene impostato a `Off` per il nuovo task.
2. La prima istruzione FPU del nuovo task genera un'eccezione `Illegal Instruction` (cause=2) con FS=Off.
3. L'exception handler intercetta questa condizione, salva il contesto FPU del vecchio owner, ripristina quello del nuovo task (se ne ha uno), imposta FS=Clean, e riprende l'esecuzione.

**Registri FPU callee-saved RISC-V** (da salvare nel context lazy):
- fs0-fs11: 12 registri × 8 byte = 96 byte
- fcsr: 4 byte (control/status)

Il campo `tc_FPURegisters[64]` in struct Task è sovradimensionato (512 byte vs. 96 necessari per i callee-saved RISC-V: fs0-fs11 + fcsr) — accettabile per ora, da ottimizzare in futuro. Il registro di controllo FPU è `tc_FCSR` (corrispondente al `fcsr` RISC-V, unico registro che incorpora fflags e frm).

---

## 4. Contratto della MMU (SV39)

### 4.1 Layout dello Spazio di Indirizzamento (QEMU Virt)

```
0x00000000_00000000 - 0x00000000_3FFFFFFF  →  Periferiche (GigaPage, R|W)
                                                UART=0x10000000, PLIC=0x0C000000
0x00000000_80000000 - 0x00000000_80FFFFFF  →  Kernel (text, rodata, data, bss, stack)
0x00000000_80600000 - 0x00000000_87FFFFFF  →  Kernel Heap (120MB su QEMU)
0x00000000_C0000000 - 0x00000000_FFFFFFFF  →  Top RAM / Framebuffer (GigaPage, R|W)
```

### 4.2 Layout dello Spazio di Indirizzamento (MilkV Mars)

```
0x00000000_00000000 - 0x00000000_3FFFFFFF  →  Periferiche (GigaPage, R|W)
0x00000000_40000000 - 0x00000000_40FFFFFF  →  Kernel
0x00000000_41000000 - 0x00000000_60FFFFFF  →  Kernel Heap (512MB)
0x00000000_C0000000 - 0x00000000_FFFFFFFF  →  Top RAM / Framebuffer (GigaPage, R|W)
```

### 4.3 Regole di Mappatura

1. **W^X è sempre rispettato** (vedi §1.2).
2. **Code heap**: dopo il caricamento ELF, le pagine vengono re-mappate da W→X. MAI W+X insieme.
3. **ASID**: ogni task U-Mode deve avere un ASID assegnato per evitare full-TLB-flush ad ogni context switch.
4. **sfence.vma**: dopo ogni modifica alle page table, eseguire `sfence.vma` con ASID specifico ove possibile.
5. **PTE_A e PTE_D**: settati manualmente nella mappatura iniziale. Alcune CPU RISC-V non li aggiornano automaticamente — la pratica corrente è corretta.

### 4.4 ASID Management (da implementare)

```c
// Ogni task U-Mode:
struct Task {
    ...
    uint16_t tc_ASID;   // Address Space ID per TLB tagging
    ...
};

// Al context switch verso un task U-Mode:
// csrw satp, (SATP_MODE_SV39 | asid << 44 | ppn)
// sfence.vma zero, asid   ← invalida solo le entry di questo ASID
```

---

## 5. Contratto del Boot

### 5.1 Sequenza di Boot (boot.S → c_start)

```
U-Boot / OpenSBI
    │
    └─→ _start (boot.S)
            │ 1. Header Linux-compatibile (per U-Boot booti)
            │ 2. Salva hartid(a0) e dtb_ptr(a1) in s0/s1
            │ 3. Disabilita interrupt (csrci sstatus, SIE)
            │ 4. Setup stack: sp = _stack_top - (hartid * 4096)
            │ 5. Imposta stvec → _exception_vector
            │ 6. Setup GP: la gp, __global_pointer$    ← REGOLA VINCOLANTE
            │ 7. Clear BSS (atomico, solo il hart vincitore del lock AMO)
            │ 8. Imposta tp = hartid (per THIS_CPU)
            │ 9. tail c_start(core_id, dtb_ptr)
            │
    c_start(core/arch/riscv64/boards/*/start.c)
            │ 1. Init UART
            │ 2. Init memory (AllocMem pools)
            │ 3. Init MMU (riscv_mmu_init)
            │ 4. Init IRQ (irq_init_system)
            │ 5. Init librerie (Exec, DOS, Graphics...)
            │ 6. Crea task IdleTask e ShellTask
            │ 7. Attiva scheduler → mai ritorna
```

### 5.2 Regole del Boot

- **`gp` deve puntare a `__global_pointer$`**, non essere azzerato. Il linker script deve esportare questo simbolo per il relaxation del compilatore.
- Il BSS viene azzerato **una sola volta** da un singolo hart. Gli altri hart aspettano con busy-wait + `fence r,r` **dentro il loop** prima di ogni lettura (FIX-008), per garantire la visibilità dello store `amoswap.rl` dell'hart vincitore.
- Lo stack viene allocato **prima** della chiamata a C (`c_start`). La dimensione per hart è 4096 byte minimo — verificare che sia sufficiente per il frame massimo del boot.
- Il `_exception_vector` è attivo dal momento del `csrw stvec`, **prima** di qualsiasi altro init. Questo è corretto e necessario.

---

## 6. Contratto del Trap Handler

### 6.1 Struttura del Frame di Eccezione (288 byte)

```c
struct ExceptionContext {
    // Offset 0: x0 (sempre 0, ma il campo esiste per indice uniforme)
    // Offset 8*1..8*31: x1..x31 (tutti i GP registers)
    uint64_t regs[32];       // 256 bytes (8*0..8*31)
    uint64_t pc;             // 8*32 = offset 256: sepc
    uint64_t status;         // 8*33 = offset 264: sstatus
    uint64_t cause;          // 8*34 = offset 272: scause
    uint64_t tval;           // 8*35 = offset 280: stval
};                           // Totale: 288 bytes
```

### 6.2 Regole del Vettore (vectors.S)

1. `SAVE_ALL` decrementa sp di 288 e salva tutti i 32 GP registers + 4 CSR.
2. `x2` (sp) salvato nel frame contiene lo **sp post-decremento** (non l'sp originale del task). Questo è intenzionale: `RESTORE_ALL` non ripristina sp dal frame, lo recupera con `addi sp, sp, 288`.
3. `x3` (gp) **non è ripristinato** in `RESTORE_ALL` — è globale al kernel e non deve cambiare. `x4` (tp) **viene ripristinato** dal frame salvato (FIX-010): i task kernel conservano hartid, i task U-Mode conservano il proprio thread pointer.
4. Il valore di ritorno di `riscv_irq_handler()` è il **nuovo stack pointer** (che può essere quello di un task differente se lo scheduler ha commutato). Il `mv sp, a0` prima di `RESTORE_ALL` implementa questo meccanismo.
5. `sret` ripristina automaticamente il livello di privilegio precedente da `sstatus.SPP`.

### 6.3 Trampoline per Nuovi Task (`_kernel_start_task`)

I nuovi task vengono avviati tramite trampoline: `context_switch` setta `ra` all'indirizzo di `_kernel_start_task`; quando la prima `ret` viene eseguita, arriviamo qui. `PrepareStack()` deve avere preparato un frame di 288 byte valido sullo stack del task prima che venga inserito nella ready list.

### 6.4 Regole dell'Exception Handler (exception.c)

- **`nesting_level` DEVE essere per-CPU** (array di MAX_CPUS, indicizzato per hartid). Un singolo intero globale non è SMP-safe.
- Il double-fault termina con `wfi` in loop, **non** con `while(1)` busy.
- La "Guru Meditation" è il meccanismo ufficiale di crash report. Il formato deve includere: nome task, causa, EPC, status, dump registri.
- Un task U-Mode che causa una fault viene rimosso dalla lista globale e lo scheduler riparte. Il sistema **non** si blocca per errori user.

---

## 7. Contratto IRQ

### 7.1 Gerarchia degli Interrupt (S-Mode)

```
scause bit 63 = 1 → Interrupt
    id=1  → Supervisor Software Interrupt (SSI) → Yield / IPI
    id=5  → Supervisor Timer Interrupt (STI)    → Scheduler tick
    id=9  → Supervisor External Interrupt (SEI) → PLIC → IRQ numerico
scause bit 63 = 0 → Exception (Trap sincrono)
    cause=8 → Environment Call from U-mode → handle_syscall()
    cause=2 → Illegal Instruction → FPU lazy trap (FIX-007: intercettato, FS=Off → restore fs0-fs11/fcsr + retry)
    altri   → _handle_exception() → Guru Meditation
```

### 7.2 Timer (SBI)

Il timer usa **SBI Legacy Extension (EID=0, FID=0)** su piattaforme con OpenSBI.
Su RISC-V 64-bit, `stime_value` è un intero a 64 bit passato **interamente in `a0`**. Il registro `a1` non è usato e non deve essere impostato.

```c
// CORRETTO per RV64:
static void sbi_set_timer(uint64_t stime_value) {
    register uintptr_t a0 asm("a0") = stime_value;
    register uintptr_t a6 asm("a6") = 0;   // FID
    register uintptr_t a7 asm("a7") = 0;   // EID legacy set_timer
    asm volatile("ecall" : "+r"(a0) : "r"(a6), "r"(a7) : "memory");
}
```

Frequenza scheduler: 100 Hz (10ms tick). Dipende da `TIMER_FREQ_HZ` definito in `platform.h` per ogni board.

### 7.3 PLIC e Interrupt Server List

Il sistema usa interrupt server list in stile Amiga: `AddIntServer(irq, &interrupt)` aggiunge un handler alla lista per quel numero di IRQ. Ogni handler è una `struct Interrupt` con un puntatore a funzione `is_Code`.

Regole:
- `EnableIRQ(irq)` deve abilitare l'IRQ sul PLIC per il **hart corrente**, non hardcoded a hart 0.
- In un sistema SMP, l'IRQ routing verso un hart specifico viene configurato tramite `plic_enable_irq(get_current_core_id(), irq)`.
- Il lock `irq_locks[irq]` protegge la lista durante il dispatch. **Non** fare spinlock nel dispatch se l'IRQ handler può bloccarsi — usare una copia locale della lista.

### 7.4 `core_id` nel Trap Handler

```c
// SBAGLIATO (attuale):
uintptr_t core_id = 0;

// CORRETTO:
uintptr_t core_id = (uintptr_t)get_current_core_id(); // legge tp
```

---

## 8. Contratto del Sistema di Memoria

### 8.1 Pool di Memoria

Exec64 usa il sistema di pool Amiga: `AllocMem(size, flags)` / `FreeMem(ptr, size)`.

| Flag | Significato |
|---|---|
| `MEMF_ANY` | Qualsiasi memoria disponibile |
| `MEMF_CLEAR` | Azzera il blocco prima di restituirlo |
| `MEMF_EXEC` | Memoria da CodeRAM (eseguibile dopo W→X flip) |
| `MEMF_DATA` | Memoria da DataRAM (dati, stack) |

Le pool sono descritte da `struct MemHeader`. `AllocMem` scorre `SysBase.ex_MemList` finché non trova una pool con memoria sufficiente.

### 8.2 Regole

- L'allocatore è protetto da spinlock (`ex_MemList` shared) — nessun alloc/free da interrupt handler senza garanzie di non-contesa.
- `FreeMem` richiede la dimensione esatta — non c'è header implicito per blocco. Questa è la filosofia Amiga: il chiamante conosce la dimensione.
- I blocchi allocati per codice ELF vengono initially mappati R+W, poi dopo il loading rimaappati R+X (mai W+X).

---

## 9. Contratto delle Librerie

### 9.1 Struttura

Ogni libreria espone:
1. `struct Library` base (nome, versione, open count).
2. Una `struct XXXInterface` con puntatori alle funzioni pubbliche.
3. Una funzione `Open()` che restituisce l'interfaccia.

L'interfaccia è stabile: i puntatori esistenti non vengono mai spostati. Nuove funzioni vengono aggiunte **in coda**.

### 9.2 Librerie Attualmente Definite

| Libreria | Interfaccia | Note |
|---|---|---|
| exec.library | `IExec` | AllocMem, AddTask, Signal, Wait, IPC |
| dos.library | `IDos` | Open, Read, Write, LoadSeg, ELF loader |
| graphics.library | `IGraphics` | Framebuffer, BitMap, Blit |
| intuition.library | `IIntuition` | Window, Screen, Gadget |
| impulse.library | `IImPulse` | Widget toolkit (pulsanti, slider...) |

---

## 10. Contratto SMP

### 10.1 Bootstrap Multi-Hart

- **Hart 0** (BSP): esegue tutto il path di init: BSS, MMU, allocatore, librerie, creazione dei task iniziali.
- **Hart 1..N** (AP): aspettano l'SBI HSM wakeup, poi saltano direttamente nel loop dello scheduler.
- Ogni hart ha il proprio stack (4096 byte × hartid, discendente da `_stack_top`).
- Ogni hart ha la propria `ExecCPU` struct (allineata a 64 byte per evitare false sharing).
- `THIS_CPU` → `&SysBase.ex_CPUs[get_current_core_id()]` → legge `tp`.

### 10.2 Discipline di Locking

| Struttura | Lock | Note |
|---|---|---|
| `ex_TaskGlobalList` | `ex_TaskLock` (spinlock) | Lista di tutti i task |
| `cpu_ReadyList` | `cpu_Lock` (spinlock) | Per-CPU, bassa contesa |
| `ex_MemList` | interno all'allocatore | AllocMem/FreeMem |
| `irq_server_lists[i]` | `irq_locks[i]` (spinlock) | Per-IRQ |
| `struct MsgPort` | `mp_Lock` (spinlock) | Messaging |
| `struct SignalSemaphore` | `ss_Lock` (spinlock) | Semafori |

**Regola fondamentale**: Non acquisire mai due spinlock contemporaneamente senza un ordine di acquisizione globale definito. L'ordine attuale è: `cpu_Lock < ex_TaskLock < mp_Lock`. Documenta ogni eccezione.

---

## 11. Stato e Issue Operative

La Foundation non mantiene un issue tracker duplicato. Bug risolti, problemi
aperti, checkpoint e risultati dei test appartengono a
`docs/STATUS/STATUS.md`; priorita e ordine delle tranche appartengono a
`docs/ROADMAP/ROADMAP.md`.

Questa separazione e normativa: uno snapshot storico contenuto nella Foundation
non deve mai essere interpretato come stato corrente del codice.

---

## 12. Roadmap Tecnica

La roadmap esecutiva autorevole e soltanto
`docs/ROADMAP/ROADMAP.md`. Deve rispettare almeno questo ordine di dipendenza:

1. completamento dell'isolamento memoria per-task
2. IPC protetto Exec-style, inclusi arena, ownership e page grant
3. cleanup autorevole delle risorse su exit e crash
4. exception path userland coerente
5. versioning e freeze append-only delle ABI pubbliche
6. completamento SMP e ottimizzazioni misurate

Le ottimizzazioni non possono aggirare i contratti del profilo selezionato.
Il fast path Performance va progettato esplicitamente, misurato e mantenuto
separato dal bookkeeping obbligatorio del profilo Protect. Una regressione che
introduce syscall, proxy, marshalling, copie o controlli dual-mode nelle chiamate
`S->S` deve essere trattata come violazione della Foundation.

---

## 13. Convenzioni del Codice

- **Linguaggio**: C (kernel) + assembly RISC-V (boot, vettori, context switch). No C++.
- **Standard C**: C11 con GCC/Clang extensions (`__attribute__`, `__builtin_*`).
- **Naming**: snake_case per funzioni e variabili, UPPER_CASE per macro e costanti, PascalCase per struct.
- **Prefissi**: `arch_` per funzioni architettura-specifica, `riscv_` per funzioni RISC-V specifiche, `sbi_` per chiamate SBI.
- **Inline assembly**: usare `asm volatile` con clobber list esplicita. Mai omettere `"memory"` quando si accede a strutture dati.
- **No stdlib**: il kernel non usa libc. Tutto quello che serve e implementato in `core/kernel/`; la libc U-Mode vive in `sdk/libc/`.
- **Commenti**: i commenti in assembly devono specificare il **contratto** (pre/post condizioni), non solo "cosa fa".

---

## 14. Piattaforme Supportate

### QEMU Virt (Piattaforma Primaria di Sviluppo)

```
Board:        qemu_virt
RAM:          1GB @ 0x80000000
Timer:        10 MHz
UART:         NS16550A @ 0x10000000 (IRQ 10)
IRQ:          PLIC @ 0x0C000000
VirtIO:       8 dispositivi MMIO @ 0x10001000..0x10008000 (IRQ 1-8)
Avvio:        qemu-system-riscv64 -machine virt -bios opensbi -kernel Exec64.elf
```

### MilkV Mars (Target Bare-Metal)

```
Board:        milkv (StarFive JH7110)
CPU:          4× SiFive U74-MC (RV64GC) @ ~1.5 GHz
RAM:          4GB @ 0x40000000
Timer:        4 MHz
UART:         NS16550A @ 0x12030000 (IRQ shift=2)
IRQ:          PLIC @ 0x0C000000
VirtIO:       Non disponibile su bare-metal
Avvio:        U-Boot booti → carica immagine come vmlinuz-5.15.0-gpu117 dalla microSD Debian
Nota:         Il kernel viene caricato sfruttando U-Boot già configurato per Debian.
              Il nome del file sarà sostituito con un'immagine custom in futuro.
```

---

*Questo documento è il riferimento normativo di Exec64. Ogni modifica al kernel che contraddica uno dei principi delle sezioni 1-10 richiede prima una revisione di questo documento con giustificazione esplicita.*
