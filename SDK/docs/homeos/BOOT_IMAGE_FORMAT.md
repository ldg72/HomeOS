# HomeOS — Formato immagine di boot

Il Core non cerca un file per nome: riceve un'immagine OS come *initrd* e la
interpreta tramite due strutture versionate. Per avviare HomeOS devi produrre un
**BootBundle** contenente il tuo ELF `Init`.

> Il **nome** del file è libero: lo decide la configurazione di boot, non il
> Core. Qui si usa `HomeOS.img`. Catena completa in `BOOT_CHAIN.md`.

## 1. Catena di boot su Milk-V Mars

```text
U-Boot / extlinux
   ├── carica il Core            (linux /Exec64)
   └── carica l'immagine OS      (initrd /HomeOS.img)
   -> il Core valida BootInfo + bundle
   -> il Core carica il payload Init (ELF64) e lo esegue
   -> HomeOS._start(Exec64InitContext*)
```

## 2. BootBundle v1.1

Header a 80 byte, little-endian, allineamento payload a 4096 byte
(`exec64/bootbundle.h`):

| Offset | Campo | Note |
|---:|---|---|
| 0 | `bb_Magic` | `EX64BNDL` (`0x4C444E4234365845`) |
| 8 | `bb_AbiMajor` | 1 |
| 10 | `bb_AbiMinor` | 1 |
| 12 | `bb_HeaderSize` | 80 |
| 16 | `bb_BundleSize` | dimensione totale del file |
| 24 | `bb_InitOffset` | offset del payload Init (allineato 4096) |
| 32 | `bb_InitSize` | dimensione del payload Init |
| 40 | `bb_SystemOffset` | offset del filesystem (0 se assente) |
| 48 | `bb_SystemSize` | dimensione del filesystem (0 se assente) |
| 56 | `bb_InitType` | `EXEC64_BOOT_PAYLOAD_ELF64` (1) |
| 60 | `bb_SystemType` | `EXEC64_BOOT_PAYLOAD_EXFAT` (2) o `NONE` (0) |
| 64 | `bb_CatalogOffset` | offset catalogo (0 se assente) |
| 72 | `bb_CatalogSize` | dimensione catalogo (0 se assente) |

Per un OS minimale senza filesystem:

```text
[ header 80 B ][ pad a 4096 ][ Init ELF64 ][ pad a 4096 ]
bb_InitOffset = 4096
bb_SystemSize = 0, bb_SystemType = EXEC64_BOOT_PAYLOAD_NONE
bb_BundleSize = fine del payload Init (allineata)
```

Il Core rifiuta magic/versione errati, offset non allineati, range in overflow e
payload non contenuti nel bundle, con errore deterministico sulla seriale.

## 3. BootInfo (prodotto dal bootstrap, non da te)

Il bootstrap della piattaforma riempie `struct Exec64BootInfo`
(`exec64/bootinfo.h`, v1.2, 112 byte) e lo passa al Core: contiene indirizzi,
dimensioni, tipo e ABI richiesta per Init e per l'eventuale sistema. Tu non lo
scrivi, ma la sua `bi_InitAbiMajor`/`bi_InitAbiMinor` deve combaciare con quella
che il tuo Init si aspetta.

## 4. File sulla partizione di boot

```text
Exec64            Core (rinominato come nella extlinux)
HomeOS.img        il tuo BootBundle
uEnv.txt          variabili U-Boot (initrd_high=... per non spostare l'initrd)
extlinux/
  extlinux.conf   label con: linux /Exec64  +  initrd /HomeOS.img
```

Gli esempi in `CORE/boot-config/` sono il punto di partenza; la direttiva
chiave è `initrd /HomeOS.img` (il caso pronto è
`CORE/boot-config/extlinux/extlinux.conf.homeos`). Il nome dopo `/` è arbitrario:
il Core riceve solo un intervallo di memoria dal Device Tree.

## 5. Produrre il bundle

In questa cartella **non e incluso un bundler** (richiesta: niente tool). Il
formato e pero completo e piccolo: un programma di poche decine di righe che
scrive l'header v1.1 e copia l'ELF all'offset 4096 e sufficiente. In alternativa
il progetto originale contiene `tools/mkexec64boot.c` come riferimento.

## 6. Verifica rapida

- il bundle inizia con i byte `45 58 36 34 42 4E 44 4C` (`EX64BNDL`);
- `bb_HeaderSize == 80`, `bb_InitOffset % 4096 == 0`;
- l'ELF Init e RISC-V64 (`ELFCLASS64`, `EM_RISCV`);
- `bb_InitSize` copre l'intero ELF.

Se il Core rifiuta il boot, la seriale riporta la causa; senza un `Init` valido
il Core entra in recovery e non avvia alcun OS.
