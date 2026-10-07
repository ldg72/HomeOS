# HomeOS — Toolchain di sviluppo

Cosa serve per compilare e avviare HomeOS su Milk-V Mars (RISC-V64).
Piattaforme supportate: **macOS** e **Linux**. **Windows non e' supportato**
(vedi in fondo).

> La toolchain **non e' inclusa** in `HomeOS/`: l'SDK distribuisce solo header,
> binari e documentazione. Va installata sulla macchina di sviluppo.

## 1. Requisiti

| Strumento | Obbligatorio | Ruolo |
|---|---|---|
| `riscv64-elf-gcc` (+ binutils `riscv64-elf-*`) | si | compila e linka Init |
| `make` (GNU) | si | build dello scheletro |
| `python3` (>= 3.6, solo stdlib) | si | bundler `HomeOS.img` |
| `qemu-system-riscv64` | no | test virtuale prima dell'hardware |
| `git` | no | versionamento del tuo progetto OS |

### Attenzione al target: `elf`, non `linux`

Serve la toolchain **bare-metal** `riscv64-elf-*`. Non usare `riscv64-linux-gnu-*`:
quella produce binari per Linux/glibc, incompatibili con un Init freestanding
che il Core carica direttamente.

La build usa: `-ffreestanding -nostdlib -march=rv64imafd_zifencei -mabi=lp64d
-mcmodel=medany -mno-relax -msmall-data-limit=0`.

## 2. macOS

Con [Homebrew](https://brew.sh):

```sh
brew install riscv64-elf-gcc        # porta anche riscv64-elf-binutils
brew install qemu                   # opzionale, per i test virtuali
```

Verificato su Apple Silicon con:

| Componente | Versione |
|---|---|
| `riscv64-elf-gcc` | GCC 15.2.0 (`--target=riscv64-elf`) |
| binutils | GNU ld 2.45.1 |
| `qemu-system-riscv64` | 10.1.0 (opzionale) |
| Python | 3.9.6 (stdlib) |
| GNU Make | 3.81 |

L'installazione tipica (Apple Silicon) mette gli eseguibili in
`/opt/homebrew/bin`, gia' nel `PATH`; su Intel sono in `/usr/local/bin`.

## 3. Linux

### Opzione A — build dalla sorgente (consigliata, riproducibile)

La sorgente di riferimento e' `riscv-collab/riscv-gnu-toolchain`, configurata
per il target **elf** (senza libc di sistema):

```sh
git clone --recursive https://github.com/riscv-collab/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix="$HOME/opt/riscv64-elf" --target=riscv64-elf
make -j"$(nproc)"
export PATH="$HOME/opt/riscv64-elf/bin:$PATH"
```

### Opzione B — pacchetto della distro

Molte distribuzioni forniscono una toolchain bare-metal. Il **prefisso dei
comandi** puo' variare: se non e' `riscv64-elf-`, passalo esplicitamente a make.

Esempio (Debian/Ubuntu, pacchetto `gcc-riscv64-unknown-elf`, prefisso
`riscv64-unknown-elf-`):

```sh
sudo apt install gcc-riscv64-unknown-elf
cd HomeOS/OS && make CROSS=riscv64-unknown-elf-
```

Il `Makefile` usa `CROSS ?= riscv64-elf-`, quindi qualsiasi prefisso funziona
con `make CROSS=<prefisso>-`.

## 4. Verifica rapida

```sh
riscv64-elf-gcc --version          # deve dire: Target: riscv64-elf
cd HomeOS/OS
make                               # -> Build/HomeOS.elf + Build/HomeOS.img
```

Output atteso:

```text
HomeOS boot bundle: Build/HomeOS.img
  Init:   offset=0x1000 size=...
  Total:  ... bytes
```

Se `make` non trova il compilatore, il prefisso e' diverso: usa `make CROSS=...`.

## 5. Test virtuale (opzionale)

QEMU permette di provare il ciclo di boot senza la scheda:

```sh
qemu-system-riscv64 --version
```

I riferimenti per QEMU e per il caricamento sono in
`../hardware/hardware_setup_qemu_milkv.md`. Il target resta comunque la
Milk-V Mars fisica.

## 6. Windows

Non supportato, per scelta di progetto: la toolchain e le procedure di questa
cartella sono pensate e verificate solo per macOS e Linux. Nessuna istruzione,
nessun supporto e nessun test sono previsti su Windows; se ti serve Windows,
usa una macchina virtuale Linux, ma ufficialmente il progetto non lo copre.

(Il progetto nasce con una forte eredita' Amiga: la filosofia e' "un sistema
che sai perche' funziona", e questa scelta di piattaforma va in quella
direzione.)
