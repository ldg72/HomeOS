# Exec64 Core Boot Contract v1

**Stato**: contratto approvato; separazione binaria e consegna Init via bundle implementate
**Data**: 2026-10-01 (contratto iniziale: 2026-08-24)
**Ambito**: confine tra bootstrap di piattaforma, Exec64 Core e immagine Exec64 OS

## Estensione catalogo, 2026-10-01

BootBundle v1.1 aggiunge offset/size catalogo a offset 64/72 (header totale 80).
BootInfo v1.2 ne trasporta il range fisico validato: la policy contenuta nella
risorsa appartiene all'OS, non e una policy hardcoded del Core. Init ABI resta
1.1. Nessuna risorsa catalogo viene consegnata a USER o usata come disco SYS.
I vecchi bundle v1.0 restano accettati; la build standard continua a generarli.
Il catalogo presente deve essere interamente valido prima della pubblicazione
dei descrittori. Dettagli in [dual_mode_boot_catalog.md](dual_mode_boot_catalog.md).

## Obiettivo

Rendere il binario Exec64 Core indipendente dal contenuto di Exec64 OS.

Il risultato atteso e che una modifica a driver, librerie, `Init`, shell,
comandi o asset possa cambiare l'immagine OS senza cambiare ne rilinkare il
binario Core.

Il contratto non decide ancora quali servizi debbano uscire dal Core. Separa
prima gli artefatti di boot, conservando il comportamento runtime corrente.

## Problema Legacy

La build Milk-V Mars legacy trasforma `Release/RISCV64/initrd.img` in `initrd.o`
e lo collega dentro `Exec64.riscv`. Il ramdisk viene poi trovato tramite simboli
generati dal linker. Il nuovo target split non esegue piu questo collegamento.

Questa composizione produce un solo file facile da avviare, ma ha tre effetti:

- ogni modifica OS cambia il file finale attribuito al Core
- Core e OS non hanno hash o cicli di rilascio indipendenti
- non e possibile distribuire il Core senza distribuire anche una specifica
  immagine SYS incorporata

## Principi Vincolanti

Il contratto rispetta la Foundation e introduce queste regole:

- `BootInfo` trasporta fatti di boot, non policy Exec64 OS
- ogni indirizzo ricevuto dal firmware viene considerato non fidato finche il
  Core non ne valida range, dimensione, overflow e sovrapposizioni
- il contratto non concede accesso diretto U-Mode a MMIO o memoria kernel
- il contratto non modifica W^X, ownership o regole di caricamento ELF
- il bootstrap di piattaforma puo essere specifico; il consumatore Core resta
  comune a QEMU e Milk-V Mars
- l'estensione della struttura e append-only e protetta da versione e size

## Flusso Previsto

```text
firmware / OpenSBI / U-Boot
             |
             v
bootstrap board-specific
  - riceve hart ID e DTB
  - individua il bundle di boot esterno
  - valida range e header del bundle
  - costruisce BootInfo v1.2
             |
             v
Exec64 Core comune
  - valida BootInfo
  - mappa i payload di boot
  - carica Init ELF64 direttamente dalla RAM
             |
             v
Init U-Mode appartenente all'OS
  - riceve la responsabilita della sessione OS
  - avvia servizi e shell secondo la policy del proprio sistema
```

Il master hart costruisce e pubblica la struttura prima di rilasciare gli hart
secondari. Tutti gli hart osservano la stessa istanza immutabile.

## Layout Logico `BootInfo v1`

L'header pubblico e definito in `core/include/uapi/exec64/bootinfo.h`. Il layout
v1 e:

| Campo | Tipo logico | Significato |
|---|---:|---|
| `magic` | 8 byte | firma ASCII `EX64BOOT` |
| `abi_major` | `u16` | versione maggiore, inizialmente `1` |
| `abi_minor` | `u16` | versione minore, inizialmente `0` |
| `struct_size` | `u32` | byte validi forniti dal producer |
| `flags` | `u64` | presenza e attributi dei campi opzionali |
| `platform_data` | `u64` | indirizzo fisico del DTB o dato firmware nativo |
| `system_image_start` | `u64` | indirizzo fisico iniziale dell'immagine OS |
| `system_image_size` | `u64` | dimensione in byte dell'immagine OS |
| `system_image_type` | `u32` | formato dell'immagine, `1` per exFAT v1 |
| `reserved0` | `u32` | deve essere zero |
| `reserved1` | `u64` | deve essere zero |

Questi campi costituiscono il prefisso immutabile `BootInfo v1.0`, lungo 64
byte. `BootInfo v1.1` aggiunge in coda:

| Campo | Tipo logico | Significato |
|---|---:|---|
| `init_image_start` | `u64` | indirizzo fisico iniziale del payload Init |
| `init_image_size` | `u64` | dimensione in byte del payload Init |
| `init_image_type` | `u32` | formato, `2` per ELF64 |
| `init_abi_major` | `u16` | major ABI richiesta da Init |
| `init_abi_minor` | `u16` | minor ABI richiesta da Init |
| `reserved2` | `u64` | deve essere zero |

Il prefisso v1.1 occupa 96 byte. La versione corrente v1.2 aggiunge
`bi_CatalogStart` e `bi_CatalogSize`, due u64 a offset 96 e 104, per un totale di
112 byte. Il flag `EXEC64_BOOTINFO_HAS_CATALOG` e il bit 4. Il consumer accetta
ancora i prefissi v1.0 e v1.1 e azzera i campi non forniti. Gli indirizzi sono indirizzi fisici
di bootstrap; non diventano automaticamente puntatori U-Mode o mapping
permanenti.

### Flag v1

- `BOOTINFO_HAS_PLATFORM_DATA`: `platform_data` e valido
- `BOOTINFO_HAS_SYSTEM_IMAGE`: start, size e type sono validi
- `BOOTINFO_SYSTEM_IMAGE_WRITABLE`: il backing RAM puo essere modificato
- `BOOTINFO_HAS_INIT_IMAGE`: il descrittore Init v1.1 e valido

I bit non riconosciuti vengono rifiutati; i campi riservati devono essere zero.

## Validazione Core Obbligatoria

Prima di usare l'immagine OS, il Core deve verificare almeno:

- magic e versione supportata
- `struct_size` minimo e allineamento della struttura
- `system_image_start + system_image_size` senza overflow
- immagine interamente contenuta in RAM utilizzabile
- nessuna sovrapposizione con Core, stack bootstrap, page table o aree riservate
- tipo immagine supportato
- dimensione minima sufficiente per il formato dichiarato
- payload Init ELF64 RISC-V compatibile e ABI Init supportata
- assenza di sovrapposizioni tra Init e immagine SYS

Un contratto non valido deve produrre un errore UART deterministico. Non deve
causare un accesso speculativo all'indirizzo fornito.

## Artefatti Correnti

La struttura di output a regime sara:

```text
Build/
|-- Exec64.riscv
|-- Core/milkv-mars/Exec64Core
|-- OS/
|   |-- qemu/Exec64Boot.img
|   `-- milkv-mars/
|       |-- Exec64Boot.img
|       |-- Exec64OS.img
|       `-- Exec64SYS.img
`-- System/Init
```

`Exec64Core` cambia solo quando cambiano sorgenti Core, runtime Core, UAPI Core,
linker o bootstrap della relativa piattaforma.

`Exec64Boot.img` cambia quando cambiano Init o uno dei payload inclusi.
`Exec64SYS.img` e il filesystem exFAT grezzo. Su Mars `Exec64OS.img` e un alias
di compatibilita byte-identico a `Exec64Boot.img`, per non richiedere modifiche
all'attuale configurazione extlinux.

Il target `image-*` compone o copia artefatti gia costruiti. Non deve rilinkare
implicitamente il Core a causa di una modifica OS.

## Milk-V Mars

Il percorso split usa due file richiesti sulla partizione `MARS_BOOT`:

```text
Exec64
Exec64OS.img
```

La configurazione U-Boot/extlinux carica `Exec64OS.img` come initrd e pubblica
start/end nel DTB. Il file e un bundle: contiene l'ELF Init e l'immagine exFAT
SYS come payload distinti e allineati. La configurazione attuale continua a
chiamare il Core `Exec64`; `image-mars` produce per questo una copia compatibile
di `Exec64Core` senza modificarne il contenuto.

Il bootstrap Mars riceve gia il puntatore DTB e lo passa al Core come platform
data. Questo consente di introdurre il contratto senza cambiare il modello
S-Mode/U-Mode.

## QEMU

QEMU carica `Build/Exec64.riscv` come kernel, il disco exFAT come drive VirtIO e
`Build/OS/qemu/Exec64Boot.img` tramite `-initrd`. Il bundle QEMU contiene solo
Init; il filesystem continua ad arrivare dal disco VirtIO. Producer e controlli
dei range restano board-specific, mentre header, parser e consumo Core sono gli
stessi della Mars.

Il launcher consente di selezionare un altro OS senza ricompilare il Core:

```sh
EXEC64_BOOT_IMAGE=Build/OS/qemu/CustomOS.img ./run.sh
```

Il file selezionato deve rispettare il contratto `EX64BOOT` e contenere un Init
compatibile con l'ABI dichiarata.

## Transizione Reversibile

Durante il bring-up devono coesistere:

- build legacy Mars con initrd incorporato
- build split sperimentale con Core e OS separati

La build legacy verra rimossa solo dopo:

- boot QEMU fino alla shell
- boot Milk-V Mars fino alla shell
- errore controllato in assenza dell'immagine OS
- verifica che due rebuild OS consecutivi non cambino l'hash del Core

## Tranche di Implementazione

### Tranche 0 - completata da questo documento

- definire ownership e layout logico di `BootInfo v1`
- definire nomi e invarianti degli artefatti
- conservare invariato il runtime

### Tranche 1 - header e plumbing inattivo (completata)

- [x] aggiungere l'header Core pubblico versionato
- [x] costruire `BootInfo` nei bootstrap QEMU e Mars
- [x] validarlo e stamparne diagnostica senza usarlo per il ramdisk
- [x] avviare QEMU fino alla shell U-Mode con diagnostica valida
- [x] confermare il boot Milk-V Mars fino alla shell su hardware reale

La build Mars legacy continua a incorporare `initrd.o`. Il plumbing non cambia
la sorgente del disco SYS e non rende ancora attiva un'immagine esterna. La
diagnostica BootInfo e stata osservata sulla console seriale Mars prima del
`CLEAR` della startup-sequence e il sistema ha raggiunto regolarmente la shell.

### Tranche 2 - immagine Mars esterna opzionale

- [x] leggere opzionalmente `linux,initrd-start/end` da `/chosen` con un parser
  FDT Mars bounds-checked
- [x] rifiutare range fuori RAM, non allineati, sovrapposti al DTB o esterni al
  corridoio tra fine Core e inizio heap
- [x] verificare la firma exFAT prima di usare il candidato come disco SYS
- [x] inizializzare il ramdisk dal contratto valido e mantenere il fallback
  embedded legacy
- [x] lasciare invariato il percorso QEMU/VirtIO
- [x] validare su Mars reale il rifiuto dell'initrd Linux corrente e il boot dal
  fallback embedded
- [x] validare su Mars reale un'immagine `Exec64OS.img` esterna

La copia corrente del DTB Mars contiene proprieta statiche
`linux,initrd-start/end` pari a `0x46100000..0x4C000000`, intervallo che include
il DTB caricato a `0x48000000`. Questo dato non viene considerato affidabile: il
bootstrap lo rifiuta deterministicamente per sovrapposizione. Se U-Boot aggiorna
le proprieta con l'initrd realmente caricato a `ramdisk_addr_r`, il candidato
supera il primo filtro solo se resta nel corridoio fisico consentito; il ramdisk
controlla poi la firma exFAT prima dell'attivazione.

Questa tranche non modifica la partizione `MARS_BOOT`, non elimina `initrd.o` e
non produce ancora i due artefatti finali indipendenti. Tali cambiamenti restano
rispettivamente lavoro di packaging successivo e obiettivo della tranche 3.

Il primo test su Milk-V Mars reale ha confermato il percorso conservativo: il
candidato non e stato pubblicato, `BootInfo v1` ha riportato
`system-image=absent`, il ramdisk ha selezionato `embedded fallback image` e il
sistema ha raggiunto regolarmente la shell U-Mode.

Il secondo test su Milk-V Mars reale ha confermato il percorso esterno: U-Boot
ha caricato `Exec64OS.img` a `0x48100000`, il ramdisk ha riportato
`Source: BootInfo external exFAT image`, il disco SYS e stato montato e la shell
U-Mode ha raggiunto il prompt. La tranche 2 e quindi validata su hardware reale.

### Tranche 3 - Core Mars realmente indipendente

- [x] rimuovere `initrd.o` dal link del nuovo target split
- [x] produrre `Exec64Core` e `Exec64OS.img` separati
- [x] aggiungere il controllo automatico dell'hash Core
- [x] conservare una build embedded esplicita e reversibile
- [x] validare su Milk-V Mars reale il nuovo `Exec64Core` split
- [x] validare l'errore controllato in assenza di `Exec64OS.img`

La validazione host conferma che `core-mars` completa senza
`Release/RISCV64/initrd.o`, non lascia simboli indefiniti e produce un Core di
circa 80 KiB. `check-core-mars-hash` ha inoltre ricostruito l'intero OS e
confermato byte per byte che il Core conserva lo stesso SHA-256. Il target
`release-milkv-legacy` mantiene separatamente il percorso embedded precedente.
Il test sulla Milk-V Mars reale ha quindi caricato la coppia split prodotta da
`release-milkv` e ha raggiunto regolarmente il prompt della shell. Il test
negativo, eseguito senza la direttiva extlinux `initrd`, ha avviato il solo Core,
rifiutato il range initrd statico sovrapposto al DTB e raggiunto la recovery
`NO_DESCRIPTOR` senza shell fallback e senza eccezioni.

### Tranche 4 - Init indipendente dal filesystem e convergenza QEMU

- [x] estendere `BootInfo` in modo append-only con il descrittore Init
- [x] definire e comporre un bundle di boot versionato e allineato a 4 KiB
- [x] caricare Init direttamente dalla memoria senza path DOS nel Core split
- [x] usare lo stesso contratto bundle su QEMU e Mars
- [x] validare su QEMU boot positivo, restart shell e recovery senza bundle
- [x] validare il nuovo bundle sulla Milk-V Mars reale
- [ ] rimuovere la copia transitoria del kernel dall'immagine OS QEMU/SYS
- [ ] eliminare il target embedded solo dopo la validazione completa

### Tranche 5 - Init ABI v1.0

- [x] definire `Exec64InitContext` come UAPI versionata da 64 byte
- [x] passare il contesto in `a0` senza esporre strutture kernel
- [x] separare il CRT Init dal CRT dei comandi
- [x] validare su QEMU prompt, `version` e nuova sessione dopo `exit`
- [x] validare lo stesso handoff sulla Milk-V Mars reale

Il layout e le regole di compatibilita sono descritti in
[exec64_init_abi.md](exec64_init_abi.md).

### Tranche 6 - Boot Resource e primo DOS/exFAT OS-side

- [x] estendere `InitContext` in modo append-only alla v1.1
- [x] consegnare a Init un handle opaco owner-bound della risorsa a blocchi
- [x] mantenere backend Core separati per VirtIO/QEMU e immagine RAM/Mars
- [x] montare SYS con il primo `exfat.handler` U-Mode
- [x] esporre il primo binding U-Mode di `dos.library`
- [x] validare su QEMU directory e caricamento ripetuto di un comando
- [x] validare lo stesso percorso sulla Milk-V Mars reale
- [ ] migrare il write path: `MakeDir` OS-side validato su QEMU; file
  write/delete e gate Mars ancora pendenti

Il contratto e descritto in
[exec64_boot_resource_abi.md](exec64_boot_resource_abi.md). In questa tranche
`dos.library` ed `exfat.handler` sono bootstrap implementation linkate dentro
Init; diventeranno moduli OS separati dopo la validazione del confine. Il Core
continua temporaneamente a inizializzare il percorso legacy e non viene ancora
alleggerito.

## Fuori Perimetro

Questo primo contratto non sposta ancora:

- `InitSupervisor` fuori da `kernel.c`
- la rimozione definitiva del DOS/exFAT legacy dal Core
- il caricamento dinamico dei moduli finali `dos.library` ed `exfat.handler`
- `utility.library`, console o font
- funzioni dalla vecchia `IExec` alla futura `exec.library`
- policy `protect` / `performance`

Questi lavori restano necessari per la separazione architetturale completa e
verranno affrontati dopo i gate QEMU e Milk-V Mars del nuovo percorso OS-side.
