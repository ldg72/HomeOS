# Lacune del Core Exec64 — note per chi sviluppa il kernel

**Data**: 2026-10-06
**Autore**: progetto HomeOS
**Come sono state raccolte**: leggendo il repository `EXEC64_multiarch` in
**sola lettura** e verificando il comportamento sulla Milk-V Mars reale.
HomeOS non modifica il Core e non usa Exec64 OS: è un OS estraneo che il Core
carica come payload `Init` in supervisor mode.

Questo documento elenca **cosa manca al Core** visti dal di fuori, con
l'evidenza nel codice e la richiesta concreta. Le osservazioni che non
riguardano lo storage sono in [CORE_NOTES_FROM_HOMEOS.md](CORE_NOTES_FROM_HOMEOS.md).

---

## Sintesi

| # | Lacuna | Impatto su un OS esterno | Richiesta |
|---:|---|---|---|
| 1 | Nessun driver di storage per la Mars | per salvare un byte serve scrivere da zero un driver SDHCI più clock e pinctrl JH7110 | un backend a blocchi reale, **oppure** dichiarare nel contratto che la piattaforma non ha storage |
| 2 | `WRITABLE` su backend `MEMORY_BACKED` | la scrittura riesce, è coerente, e non persiste: un OS crede di aver salvato | documentare la volatilità, o separare il flag dalla proprietà del supporto |
| 3 | Scrittura file completa solo nel DOS legacy, non nella Boot Resource | due strade per lo stesso volume, con completezza diversa | chiudere il gate già in roadmap, o dichiarare la Boot Resource block-only |
| 4 | `SysBase->ex_IDos` è l'unica via per leggere file dal bundle, e non è versionata | un OS esterno si aggancia a un layout interno senza versione né magic | una capability di lettura versionata nei servizi di Init |
| 5 | Il profilo di Init passa da `SYS:Prefs/default.prefs` | un OS estraneo deve fornire un volume exFAT per un file da 30 byte | spostare la policy nel bundle |
| 6 | Il writer exFAT legacy è dichiarato fragile dal Core stesso | chi ci appoggia dati persistenti rischia corruzione silenziosa | chiarire lo stato del bug, o correggerlo |

---

## 1. Sulla Mars non esiste un percorso verso il supporto fisico

Il Core lo dichiara da sé:

```
[KERNEL] Milk-V Mars: Hardware storage disabled. Searching for Initrd...
```

(`core/riscv64/kernel/kernel.c:998`)

In tutto il Core l'unico dispositivo a blocchi è il **ramdisk**, cioè memoria:
`core/riscv64/kernel/dos/handlers/ramdisk_device.c`. Non esiste alcun file
SDHCI, MMC o altro driver di storage — verificato con una ricerca su tutto
`core/`.

**Conseguenza per un OS esterno**: la persistenza non è "da collegare", è **da
scrivere**. Chi vuole salvare dati sulla microSD deve implementare il
controller DesignWare MSHC del JH7110, i clock, il pinctrl e la sequenza di
inizializzazione della scheda SD, dentro il proprio OS. È un lavoro stimabile
in settimane, non in ore.

**Richiesta**: o un backend a blocchi reale dietro la Boot Resource (vedi §3),
oppure una riga esplicita nel contratto di boot: *su questa piattaforma il
Core non fornisce storage; la persistenza è responsabilità dell'OS*. La seconda
costa zero e toglie di mezzo ogni ambiguità.

---

## 2. `WRITABLE` + `MEMORY_BACKED`: scrivere non significa salvare

Come arriva il "disco" oggi, nell'ordine in cui accade:

1. U-Boot carica il bundle come initrd; il Core ne legge il range dal device
   tree (`linux,initrd-start` / `linux,initrd-end`).
2. Il bundle contiene un'immagine exFAT (`bb_SystemOffset`).
3. `ExecBootBundleDescribe` pubblica il descrittore e — quando c'è
   un'immagine di sistema — accende **incondizionatamente** due flag:

   ```c
   info->bi_Flags |= EXEC64_BOOTINFO_HAS_SYSTEM_IMAGE |
                     EXEC64_BOOTINFO_SYSTEM_IMAGE_WRITABLE;
   ```

   (`core/riscv64/kernel/bootbundle.c:157`)
4. `ExecBootResourceGrant` traduce il flag in una proprietà della risorsa:

   ```c
   BootResource.backend = BOOT_RESOURCE_BACKEND_MEMORY;
   BootResource.memory_base = (uintptr_t)boot_info->bi_SystemImageStart;
   BootResource.flags = READABLE | MEMORY_BACKED;
   if (boot_info->bi_Flags & EXEC64_BOOTINFO_SYSTEM_IMAGE_WRITABLE)
       BootResource.flags |= WRITABLE;
   ```

   (`core/riscv64/kernel/bootresource.c:66`)
5. La MMU mappa la regione scrivibile (`mmu.c:291`), e Init riceve un handle
   con `READABLE | WRITABLE | MEMORY_BACKED`.

**Il punto**: `WRITABLE` qui non descrive il supporto, descrive **la copia in
RAM**. Il flag non cambia mai in base al fatto che esista o meno un supporto
fisico: sulla Mars è sempre acceso, perché il bundle contiene sempre
un'immagine.

Il flag `MEMORY_BACKED` c'è e dice la verità, ma:

- `docs/design/exec64_boot_resource_abi.md` lo descrive come *"backing fornito
  dal bundle di boot in RAM"* senza dire che le scritture **non vengono mai
  propagate da nessuna parte**, e senza dire che non esiste alcuna operazione
  di flush;
- la combinazione `WRITABLE | MEMORY_BACKED` non è spiegata: un OS che la legge
  la interpreta naturalmente come "posso scrivere, quindi posso salvare".

**Richiesta**: una riga nel contratto e nel commento dell'header —
`MEMORY_BACKED` implica **volatilità**: le scritture restano nella copia in RAM
e si perdono allo spegnimento; non esiste flush. In alternativa, un flag
dedicato (`..._VOLATILE`) o un campo esplicito di *backing*, così che l'OS
possa decidere senza interpretare.

---

## 3. Due percorsi di scrittura, con completezza diversa

Oggi esistono **due** implementazioni che scrivono sullo stesso volume, e non
sono allo stesso stadio.

**Percorso A — DOS legacy (completo, ma su RAM).** `exfat.h:124-127` dichiara
`exfat_create_directory_entry`, `exfat_create_directory`, `exfat_write_file` e
`exfat_delete_file`; le implementazioni sono in `exfat_handler.c:91`, `:240`,
`:376`, `:485`, con allocazione e liberazione di cluster. Tutto l'I/O passa da:

```c
#define SYS_READ_SECTOR(s, b)  ramdisk_read_sync(s, b)
#define SYS_WRITE_SECTOR(s, b) ramdisk_write_sync(s, b)
```

(`exfat_handler.c:14-17`)

Quindi: il percorso è funzionalmente completo, crea file davvero, aggiorna FAT
e allocation bitmap — **nella copia in RAM**. Il livello DOS sopra ci costruisce
`MODE_NEWFILE` (`io.c:47`) e il flush alla chiusura (`io.c:71`).

**Percorso B — Boot Resource (parziale).** L'ABI OS-side ha migrato solo
`MakeDir`; il resto è dichiarato pendente dal progetto stesso:

> *"Dal checkpoint rev 0.124, `MakeDir` usa inoltre la write capability […]
> File write e delete non sono ancora migrati."*
> — `docs/design/exec64_boot_resource_abi.md:§6`
>
> *"[ ] write path OS-side: `MakeDir` validato su QEMU; file write/delete
> pendenti"* — stesso documento, §7.

**Perché conta**: un OS esterno che vuole salvare un file ha davanti due
strade, e la più completa è quella che il progetto considera legacy e
transitoria. La strada "giusta", quella versionata, è quella incompleta.

**Richiesta**: chiudere il gate già previsto dalla roadmap, con la stessa
sensibilità che ha `MakeDir` (write autorizzata solo se il backend dichiara
`WRITABLE`). Se invece la scelta di progetto è che la Boot Resource resti
**block-only** e che il filesystem sia responsabilità dell'OS, va scritto nel
contratto: è una risposta legittima e ci risparmia di costruire su una strada
in salita.

---

## 4. L'unica via per leggere file dal bundle non è un'ABI

Un OS in S-Mode non ha syscall (§1 di `CORE_NOTES_FROM_HOMEOS.md`). Per
leggere un file dal volume di boot l'unica strada è agganciarsi a:

```c
struct Interface *ex_IDos;   /* exec_base.h */
```

popolato da `dos_init()` (`core/riscv64/kernel/dos/library.c:125`) e definito
come struttura in `sdk/include/dos/dos.h:173` — con `Open`, `Read`,
`ReadEntries`, `ResolvePath`, eccetera.

**Il problema**: quella struttura non ha magic, non ha versione, non ha
`StructSize`. È un layout interno. Se in una revisione futura cambia l'ordine
dei campi, un OS esterno non se ne accorge: chiama il puntatore sbagliato e va
in trap, senza alcuna diagnostica utile. È esattamente il tipo di dipendenza
che il resto del contratto di boot evita con cura (magic, ABI major/minor,
`StructSize`, validazione deterministica).

**Richiesta**: o pubblicare `DosInterface` come interfaccia versionata (magic +
versione + `StructSize`, come `Exec64InitContext`), oppure offrire nei servizi
di Init una capability minima e versionata "leggi questo file dal bundle".
Non serve il modello DOS completo: serve un modo stabile di leggere i propri
file di avvio.

---

## 5. La policy del profilo vive dentro il filesystem di un OS

Il profilo di Init (`protect` / `performance`) non si sceglie con il catalogo
di boot: il Core monta l'immagine exFAT e legge
`SYS:Prefs/default.prefs`. Se il file manca, il default è `protect`, e un
payload che si aspetta il supervisor riceve un contesto U-Mode.

Per avviare un OS **completamente estraneo** serve quindi fornire un volume
exFAT con dentro quel file: nel nostro bundle sono **2 MiB di exFAT per 30
byte di testo**. Inoltre `SYS:Prefs/...` è un riferimento al modello
Amiga/Exec64 OS dentro il Core, mentre il contratto dichiara che la policy
appartiene al bundle.

**Richiesta**: spostare la scelta del profilo nel bundle (campo dell'header,
purché la ABI resti compatibile) o in `BootInfo`. È già un lavoro in corso
da parte vostra: questa nota serve a dire perché, dal di fuori, si sente.

---

## 6. Il writer exFAT legacy è dichiarato fragile dal Core stesso

In `core/riscv64/kernel/kernel.c:1007` c'è un commento da cui si impara molto:

```c
// NOTE: exfat_write_file removed — writing on EVERY boot corrupts the exFAT
// cluster allocation and overwrites virtiogpu.device/input.device on the disk.
```

Chi legge questo, da fuori, trae due conclusioni:

1. il writer legacy ha **bug noti di allocazione** in grado di corrompere il
   volume;
2. esiste (o esisteva) un caso in cui scrivere a ogni boot distruggeva file
   altrui.

**Perché conta per noi**: la strada più corta verso la persistenza sarebbe
appoggiarsi a quel writer e copiare poi l'immagine su un supporto. Ma se il
writer corrompe silenziosamente l'allocazione, la corruzione diventa
**persistente**: molto peggio che perderla a ogni riavvio. Da fuori non
sappiamo se il bug è stato corretto, se riguarda solo l'uso ripetuto, o se è
ancora presente.

**Richiesta**: una nota di stato nel codice o nella documentazione. "Non
usare per dati che contano" è un'informazione preziosa quanto una correzione.

---

## Cosa non chiediamo

Non chiediamo che il Core diventi un layer di policy filesystem. Il confine
attuale — *il Core non interpreta directory, FAT o file* — è giusto, ed è la
ragione per cui un OS estraneo può esistere senza adottare il modello Amiga.

Le richieste di questo documento stanno tutte **dentro** quel confine: un
backend a blocchi vero, flag che dicono la verità, un'interfaccia versionata
per leggere i propri file, la policy nel bundle.

---

## Come riprodurre le verifiche

Tutto quello che è affermato qui si controlla leggendo il repository, senza
hardware:

```sh
rg -n "Hardware storage disabled" core/riscv64
rg -n "SYSTEM_IMAGE_WRITABLE"      core/riscv64
rg -n "BOOT_RESOURCE_BACKEND_MEMORY|MEMORY_BACKED" core/riscv64
rg -n "exfat_write_file|exfat_delete_file" core/riscv64/kernel/dos/handlers
rg -n "ramdisk_write_sync"         core/riscv64/kernel/dos/handlers
rg -n "MODE_NEWFILE|io_Write|io_Close" core/riscv64/kernel/dos/io
rg -n "ex_IDos" core/riscv64/kernel/dos/library.c
find core -iname "*sd*" -o -iname "*mmc*" -o -iname "*disk*"
```

Le due righe di log che descrivono la situazione sulla scheda, in un boot reale:

```
[BOOTINFO] external system image candidate start=0x000000004810F000 size=0x0000000000200000; activation pending
[RAMDISK] Initializing Initrd Device...
  Source: BootInfo external exFAT image
```

---

## Riferimenti

- [CORE_NOTES_FROM_HOMEOS.md](CORE_NOTES_FROM_HOMEOS.md) — le altre lacune
  osservate: syscall assenti in S-Mode, `RAM_SIZE` fisso a 4 GiB, heap da
  512 MB adiacente al framebuffer, framebuffer "di U-Boot" fuorviante, refuso
  nel font, versione del Core non leggibile, reset SBI non utilizzabile.
- `docs/design/exec64_boot_resource_abi.md` — l'ABI della risorsa di boot,
  con la lista dei gate di rimozione del percorso legacy.
