# Exec64 - Input Backend e Contratto DMA Multipiattaforma

**Stato**: decisione approvata a `rev 0.110`, binding statico a `rev 0.111`, validazione tastiera VirtIO a `rev 0.112`
**Data**: 2026-07-12
**Ambito**: input comune su QEMU/VirtIO e Milk-V Mars/xHCI, con predisposizione dual-mode

---

## 0. Autorita e governance

Questo documento formalizza una decisione discussa con il maintainer durante la
lavorazione di `ROADMAP_USB_KEYBOARD_MARS.md`.

Non sostituisce:

- `EXEC64_KERNEL_FOUNDATION.md`
- `ROADMAP/ROADMAP.md`
- `STATUS/STATUS.md`

Le future modifiche al core necessarie per il contratto DMA, il registry di
piattaforma o il canale input comune devono essere nuovamente tracciate nello
status master come modifiche scaturite dalla roadmap USB/input.

---

## 1. Decisione principale

Exec64 deve usare lo stesso core kernel sui due target attivi:

- QEMU RISC-V con periferiche VirtIO
- Milk-V Mars bare-metal con periferiche JH7110 reali

Il kernel core non deve quindi conoscere direttamente:

- tastiere VirtIO
- tastiere USB HID
- dettagli xHCI Mars
- keymap specifiche di un trasporto

Deve invece offrire contratti generici per:

- MMIO e IRQ assegnati
- allocazione e sincronizzazione DMA
- consegna di eventi input canonici

VirtIO e xHCI sono backend differenti dello stesso sistema, non due architetture
kernel differenti.

---

## 2. Architettura a livelli

```text
Shell / Console / Intuition / applicazioni
                    |
              input.device
       coda, keymap, InputEvent, LED
              /             \
   VirtIO input backend   USB HID keyboard backend
            |                      |
       VirtIO MMIO          USB host / xHCI backend
            |                      |
         QEMU virt             Milk-V Mars
```

### Livello comune: `input.device`

Responsabilita:

- mantenere la coda degli eventi
- esporre eventi `InputEvent` stabili
- convertire keycode canonici in caratteri tramite keymap
- distribuire input a shell, console e Intuition
- gestire stato logico come modifier e LED

Non deve contenere accessi MMIO VirtIO o xHCI.

### Backend input

Un backend produce eventi canonici e li consegna a `input.device`.

Backend previsti:

- `virtio-input` per QEMU
- `usb-hid-keyboard` per Mars e futuri host USB
- seriale/UART come sorgente console e fallback testuale

Per i due target attuali il backend non e selezionato a runtime. Il build collega
esattamente una implementazione hardware dentro `input.device`:

```text
BOARD=qemu_virt -> input_core.o + virtinput.o
BOARD=milkv     -> input_core.o + input_mars.o
```

Il confine interno corrente richiede un solo simbolo di bootstrap statico,
`input_backend_start()`. Il backend conserva init, task, IRQ, MMIO e DMA; quando
ha estratto un evento chiama il core comune. Non viene introdotta una vtable o
una registry dinamica finche due casi hardware reali non la rendono necessaria.

### Backend di trasporto

Il backend HID non deve incorporare per sempre il controller Mars.

La separazione obiettivo e:

- HID boot keyboard: protocollo e report comuni
- xHCI: controller host USB
- JH7110/Mars: clock, reset, PHY, VBUS e routing board-specific

`marskbd` resta il laboratorio temporaneo che permette di scoprire e validare
questi confini prima della modularizzazione definitiva.

---

## 3. Binding di piattaforma e risorse

La piattaforma seleziona il backend in fase di build senza cambiare il kernel
core o l'ABI pubblica di `input.device`.

Su QEMU il backend continua a usare lo scan VirtIO gia esistente in
`ExecBase.ex_VirtIODeviceList`.

Sulla Mars il backend puo inizialmente usare le risorse JH7110/Mars note e
validate da `marskbd`. Non serve introdurre una registry hardware universale.

Se un futuro target concreto richiedera descrittori comuni, questi dovranno
restare minimi e rappresentare soltanto risorse realmente condivise, per esempio
MMIO, IRQ, DMA mask e coerenza dichiarata.

L'assenza di un backend su una piattaforma non deve richiedere un fork del
kernel core.

---

## 4. Contratto DMA comune

VirtIO e xHCI usano entrambi strutture condivise tra CPU e device. Il contratto
DMA non puo restare duplicato nei singoli driver.

Il modello deve distinguere:

- indirizzo CPU
- indirizzo DMA visto dal device
- dimensione
- allineamento
- limite di indirizzamento, per esempio DMA32
- direzione del trasferimento
- coerenza hardware oppure sincronizzazione software

Forma concettuale, non ancora ABI congelata:

```c
struct ExecDMABuffer {
    void    *cpu_addr;
    uint64_t dma_addr;
    uint32_t size;
    uint32_t alignment;
    uint64_t address_mask;
    uint32_t flags;
};
```

Primitive da valutare mantenendo, dove utile, i nomi ExecSG:

- allocazione e rilascio di buffer DMA
- `CachePreDMA()`
- `CachePostDMA()`
- conversione o mapping CPU address -> DMA address
- eventuale lista scatter/gather futura

Regole:

- `MEMF_SHARED` non deve significare implicitamente "sempre coerente"
- un semplice `fence` RISC-V non va trattato come sostituto universale del cache maintenance
- l'identity map attuale semplifica QEMU e Mars, ma resta un dettaglio della piattaforma
- un driver non deve assumere che indirizzo virtuale e indirizzo DMA coincidano
- xHCI Mars dichiara `AC64=no`, quindi il primo backend richiede indirizzi DMA sotto 4 GiB
- una futura IOMMU o UMA non deve richiedere la riscrittura del driver input

Questa direzione e coerente con `unified_memory_foundation.md`.

---

## 5. Dual-mode

Il contratto deve restare identico nei profili Exec64.

### Protect mode

- backend in `U-Mode`
- MMIO, IRQ e DMA mediati da capability/syscall validate
- fault containment del driver

### Performance mode

- stesso backend logico caricabile in `S-Mode`
- accesso diretto alle primitive autorizzate
- nessun bridge superfluo nel path caldo

### Profilo misto

- `input.device` e un backend possono vivere in domini differenti
- la policy di caricamento decide il privilege level
- ABI ed eventi non cambiano con il profilo

Il livello di privilegio e una policy di deployment, non parte del protocollo
VirtIO, xHCI o HID.

---

## 6. Stato corrente

Al checkpoint `rev 0.110`:

- `input.device` contiene direttamente il backend VirtIO
- keymap, coda shell, dispatch Intuition e MMIO VirtIO sono nello stesso file
- la shell usa ancora `SYS_KGETC`
- `SYS_KGETC` legge direttamente dalla UART della board
- `MEMF_SHARED` esiste, ma non costituisce ancora un contratto DMA completo
- il backend VirtIO RISC-V usa un `fence`, non una astrazione cache/DMA comune
- `marskbd` ha validato il path Mars fino al reset della tastiera low-speed

Dopo il checkpoint, la Fase 1 separa questo stato senza modificare il kernel:

- `input_core.c` contiene coda, keymap e dispatch `InputEvent`
- `virtinput.c` conserva invariati virtqueue, MMIO, IRQ, task e cache fence QEMU
- `input_mars.c` e un binding intenzionalmente inerte in attesa del codice xHCI
  validato da `marskbd`
- `Makefile.device` collega un solo backend scelto tramite `BOARD`
- non esistono vtable, probe universali o selezione runtime del backend

La build dei due moduli e validata, una build QEMU pulita arriva alla shell e la
release Milk-V Mars avvia correttamente shell e input seriale dal Mac. Il backend
Mars passivo non interferisce quindi con UART o bootstrap.

Il comando diagnostico U-Mode `inputprobe` apre `input.device` tramite il bridge
esistente `OpenDevice()`, attende eventi per cinque secondi e richiude il device.
Su QEMU con una sola `virtio-keyboard-device` MMIO sono stati confermati:

- apertura e chiusura del device senza fault
- avvio stabile del task del backend
- ricezione di un evento tastiera VirtIO reale iniettato dal monitor QEMU

Il vecchio `run.sh` esponeva mouse e tastiera come device PCI, invisibili allo
scan VirtIO MMIO di Exec64. Il profilo QEMU corrente usa ora due device MMIO,
`virtio-keyboard-device` e `virtio-tablet-device`. La configurazione simultanea
e stata validata con apertura unica di `input.device`, inizializzazione dei due
backend e ricezione di eventi tastiera e puntatore senza fault.

`input.device` espone inoltre il comando sincrono `INPUT_CMD_READ_EVENT`: il
consumer U-Mode legge da una coda canonica comune eventi `SYN`, `KEY`, `REL` e
`ABS`. Il driver non chiama direttamente librerie U-Mode; la traduzione verso
`intuition.library` appartiene al processo grafico che possiede la sessione.

Da `input.device` 2.1 e disponibile anche `INPUT_CMD_READ_EVENTS`, estensione
append-only che usa i campi standard di `IORequest`: `io_Data` indica un array
di `ExecInputEvent`, `io_Length` la capacita in byte e `io_Actual` i byte
restituiti. La lettura singola resta invariata. Il batch svuota la coda sotto un
solo lock e permette a Intuition di attraversare una sola volta il confine
`SYS_DO_IO` per tutti gli eventi gia disponibili.

### Mitigazione polling QEMU (2026-08-25)

Un test Cocoa prolungato ha mostrato che il flusso IRQ del tablet puo arrestare
l'intero consumo degli eventi e far emergere un resume supervisor con
`KernelRoot_Master`, `Cause=12` ed `EPC=0`. La prova `loadgfx nocursor` ha
eliminato ogni movimento del cursor plane senza recuperare `Esc`, escludendo la
cursor queue come causa residua.

In attesa di una tranche Core esplicitamente discussa, `virtinput.c` usa quindi
una mitigazione OS reversibile:

- `VIRTQ_AVAIL_F_NO_INTERRUPT` sulle virtqueue di tastiera e tablet
- nessuna registrazione PLIC dei due IRQ input
- polling adattivo delle used ring: 10 ms durante l'attivita e 40 ms dopo tre
  letture vuote; il device viene notificato soltanto quando sono stati
  riciclati descriptor

Questo mantiene la latenza limitata a un tick corrente ed evita che ogni
campione del puntatore attraversi il percorso `IRQ -> Signal -> ScheduleTick`.
La prova fisica con movimento intermittente, permanenza prolungata nella
sessione grafica e successiva ripresa dell'input e terminata senza blocchi o
Guru. Il polling adattivo e pertanto la mitigazione stabile corrente, ma non rappresenta
ancora la policy IRQ definitiva del backend VirtIO.

### Batch input U-Mode (2026-09-12)

Il benchmark con movimento continuo del puntatore ha misurato 3.304
`SYS_DO_IO`, 7.652 attivazioni MMU e 1.044 cambi task nel campione manuale. La
relazione `7652 = 1044 + (2 * 3304)` conferma che il costo dominante del
percorso attivo era una lettura sincrona per ogni singolo evento.

Intuition usa quindi `INPUT_CMD_READ_EVENTS` e processa fino a 128 eventi con
una sola `DoIO`, mantenendo il coalescing dell'aggiornamento finale del
puntatore. Questa modifica riguarda esclusivamente l'ABI append-only di
`input.device` e il consumer OS: non cambia Core, syscall ABI o formato degli
eventi esistenti. Il percorso singolo resta disponibile per compatibilita.

---

## 7. Migrazione incrementale

### Fase 0 - checkpoint

- congelare `rev 0.110`
- non iniziare il bootstrap DMA xHCI prima del checkpoint
- preservare la baseline QEMU funzionante

### Fase 1 - estrazione input core senza cambio comportamento

- [x] separare coda, keymap e dispatch eventi dal backend VirtIO
- [x] mantenere invariato il percorso hardware QEMU
- [x] aggiungere un solo binding statico interno
- [x] aggiungere un backend Mars inerte, senza integrazione xHCI
- [x] confermare boot e shell con una build QEMU pulita
- [x] confermare boot Mars e shell UART su hardware reale
- [x] aprire `input.device` e confermare un evento VirtIO reale su QEMU
- [x] correggere e validare l'inizializzazione contemporanea di tastiera e
  tablet VirtIO MMIO

### Fase 2 - contratto DMA/cache

- [x] definire le direction minime `TO_DEVICE`, `FROM_DEVICE`, `BIDIRECTIONAL`
- [x] aggiungere il ponte U-Mode append-only `SYS_DMA_SYNC=46`
- [x] validare ownership, allineamento e dimensione del range nel kernel
- [x] implementare fence di ordinamento per i percorsi coerenti Mars e QEMU;
  il test reale ha escluso Zicbom sui core U74
- [ ] trasformare il ponte diagnostico in `CachePreDMA()` / `CachePostDMA()` o
  contratto dual-mode definitivo solo dopo il test xHCI reale

### Fase 3 - bootstrap xHCI nel laboratorio Mars

- [x] mantenere `marskbd` come comando diagnostico
- [x] allocare DCBAA, scratchpad, command ring ed event ring tramite il contratto DMA
- [x] usare polling iniziale
- [x] validare un No-op Command TRB con cleanup halted/reset su hardware reale
- [ ] validare `Enable Slot` e command completion

### Fase 4 - HID e backend Mars

- completare EP0 e descriptor
- leggere report HID boot keyboard
- fattorizzare protocollo HID dal controller xHCI
- consegnare eventi canonici a `input.device`

### Fase 5 - console e shell

- rimuovere la dipendenza logica della shell dalla UART diretta
- usare il canale input comune
- mantenere la UART come backend/fallback seriale
- validare contemporaneamente QEMU/VirtIO e Mars/USB

---

## 8. Matrice minima di validazione

| Target | Backend | Test obbligatorio |
|---|---|---|
| QEMU | VirtIO keyboard MMIO | apertura device e ricezione evento reale |
| QEMU | UART | shell seriale ancora utilizzabile |
| Mars | UART | shell seriale disponibile durante tutto il bring-up |
| Mars | xHCI + HID | keycode e modifier visibili in polling |
| Mars | input core | caratteri consegnati alla shell |

Ogni fase che cambia il contratto comune deve verificare almeno QEMU e Mars.

---

## 9. Decisioni esplicite

- niente codice xHCI nel kernel core
- niente MMIO VirtIO nel core di `input.device`
- niente assunzione permanente di identity DMA
- niente fork del kernel per QEMU e Mars
- niente integrazione shell Mars tramite scorciatoia separata
- `marskbd` resta temporaneo e reversibile
- il risultato finale usa lo stesso `input.device` e gli stessi eventi su entrambi i target
