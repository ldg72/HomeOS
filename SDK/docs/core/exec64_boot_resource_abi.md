# Exec64 - Boot Resource ABI v1

**Stato**: v1.0 implementata; lettura validata su QEMU e Milk-V Mars
**Autorita**: [../ROADMAP/ROADMAP.md](../ROADMAP/ROADMAP.md)
**UAPI**: `core/include/uapi/exec64/bootresource.h`

## 1. Scopo

La Boot Resource ABI consegna a `Init` una capability opaca per la risorsa a
blocchi da cui l'OS puo montare il proprio sistema. Il Core seleziona il backend
di piattaforma, ma non espone a U-Mode indirizzi fisici, MMIO, strutture VirtIO
o strutture private del kernel.

Il primo consumer Exec64 OS usa questa capability per attivare in U-Mode:

- `exfat.handler`, che interpreta il filesystem
- `dos.library`, che espone nomi, assign e API DOS in stile AmigaOS

Il formato exFAT e un fatto della risorsa corrente, non una policy filesystem
del Core. Il Core trasferisce blocchi e non interpreta directory, FAT o file.

## 2. Handle e lifetime

`Exec64BootResourceHandle` e un valore opaco a 64 bit. Il valore zero e sempre
invalido.

- il Core crea l'handle per il task Init
- soltanto il task proprietario puo usarlo
- l'handle non e un indirizzo e non puo essere dereferenziato
- il Core lo revoca quando Init termina o il relativo handoff fallisce
- una nuova istanza Init riceve una nuova generazione dell'handle
- Init puo delegare servizi a componenti figli solo attraverso future capability
  esplicite; la v1 non rende l'handle trasferibile

## 3. Descrittore v1.0

`Exec64BootResourceInfo` occupa 48 byte:

| Offset | Campo | Tipo | Significato |
|---:|---|---|---|
| 0 | `bri_AbiMajor` | `u16` | major ABI |
| 2 | `bri_AbiMinor` | `u16` | minor ABI |
| 4 | `bri_StructSize` | `u32` | byte validi |
| 8 | `bri_Type` | `u32` | `BLOCK` nella v1 |
| 12 | `bri_Format` | `u32` | formato dichiarato, oggi `EXFAT` |
| 16 | `bri_Flags` | `u32` | attributi della risorsa |
| 20 | `bri_BlockSize` | `u32` | dimensione blocco, oggi 512 byte |
| 24 | `bri_ByteSize` | `u64` | capacita totale in byte |
| 32 | `bri_Reserved0` | `u64` | zero |
| 40 | `bri_Reserved1` | `u64` | zero |

Flag v1:

- `READABLE`: lettura consentita
- `WRITABLE`: scrittura consentita dal backend
- `MEMORY_BACKED`: backing fornito dal bundle di boot in RAM

## 4. Operazioni Core

La tabella canonica `sdk/include/exec/syscalls.h` assegna:

| Syscall | Numero | Operazione |
|---|---:|---|
| `SYS_BOOT_RESOURCE_INFO` | 47 | copia il descrittore pubblico |
| `SYS_BOOT_RESOURCE_READ` | 48 | legge blocchi nel buffer U-Mode |
| `SYS_BOOT_RESOURCE_WRITE` | 49 | scrive blocchi se autorizzato |

Ogni trasferimento deve essere:

- allineato alla dimensione blocco
- multiplo della dimensione blocco
- non nullo e non superiore a 64 KiB
- interamente compreso nella capacita dichiarata
- diretto verso un buffer U-Mode validato dal dispatcher

Range overflow, handle scaduti, owner errato o write su risorsa read-only
producono errore senza effettuare I/O.

## 5. Backend correnti

- QEMU: disco VirtIO block, capacita letta dalla configurazione del device
- Milk-V Mars: immagine SYS descritta da `BootInfo`, gia validata e mappata dal
  Core prima dell'handoff

Questa differenza resta confinata nel Core. `dos.library` ed `exfat.handler`
usano la stessa ABI e non contengono controlli di piattaforma.

## 6. Stato transizionale

Il primo `dos.library` ed `exfat.handler` sono compilati nel payload Init per
validare il confine funzionale prima del caricamento dinamico definitivo. Non
sono ancora due moduli separati caricati da disco.

I task U-Mode separati non ricevono la capability. Quando il binding DOS
transizionale deve leggere o scrivere un blocco non gia presente nella cache,
inoltra a `Init` una richiesta interna bounded. `Init`, proprietario
autorevole del `BootResource`, esegue l'operazione e copia al massimo 32 KiB
per messaggio; una richiesta pubblica da 64 KiB viene segmentata in due
messaggi. Questo proxy evita il fallback sincrono al DOS legacy senza rendere
trasferibile l'handle e verra assorbito dal futuro servizio DOS U-Mode.

Il percorso OS-side implementa lettura, directory, resolve e caricamento ELF.
Dal checkpoint `rev 0.124`, `MakeDir` usa inoltre la write capability quando il
backend dichiara `EXEC64_BOOT_RESOURCE_WRITABLE`. Le mutazioni DOS non ancora
migrate possono delegare al DOS/exFAT legacy del Core. Tale fallback resta
temporaneo e non fa parte della ABI finale.

Su QEMU e Milk-V Mars sono stati validati mount, `dir`, caricamento ed
esecuzione ripetuta di `hexdump`. Il conteggio dello spazio exFAT viene ricavato
dalla allocation bitmap attraverso la stessa Boot Resource e sulla Mars ha
riportato `2 MiB free of 22 MiB (86% used)`. La scrittura legacy ripetuta con
`MakeDir` OS-side e stato validato su QEMU con directory semplice, duplicato,
directory annidata, riapertura host e `fsck_exfat`. Il backend Mars non espone
ancora il flag writable e continua quindi a usare il fallback. File write e
delete non sono ancora migrati.

## 7. Gate di rimozione legacy

- [x] assert host su layout e offset
- [x] QEMU: mount SYS tramite Boot Resource
- [x] QEMU: directory e caricamento comando ripetuto
- [x] Milk-V Mars: mount SYS tramite backend memory-backed
- [x] Milk-V Mars: directory e caricamento comando ripetuto
- [ ] write path OS-side: `MakeDir` validato su QEMU; file write/delete pendenti
- [ ] moduli finali `dos.library` ed `exfat.handler` caricabili dall'OS
- [ ] rimozione DOS/exFAT legacy dal Core solo dopo i gate precedenti
