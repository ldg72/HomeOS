# Exec64 Technical Reference

**Last Updated**: February 13, 2026
**Purpose**: Comprehensive technical reference covering ELF loading, memory layout, and build procedures

---

## CHAPTER 1: ELF Loading Strategy

### The Problem: "Bare Metal" Toolchains vs. Dynamic Linking

Standard GCC cross-compilers for embedded development (e.g., `riscv64-elf-gcc`) are designed for static binaries. They often lack the support libraries (`crtbeginS.o`, `ld.so`) required to build standard "Shared Objects" (`.so` / `ET_DYN`).

When attempting to build with `-shared`:
1.  The linker may fail outright ("-shared not supported")
2.  It may produce a binary without a Global Offset Table (GOT) or dynamic relocations (`.rela.dyn`), making it impossible to load at arbitrary addresses

### The Solution: "Static PIE with Load-Time Relocation"

Instead of fighting the toolchain to produce a standard shared object, we trick it into producing a **Relocatable Static Executable**.

#### Build Flags Configuration

We compile the library with a specific set of flags (`sdk/libc/Makefile.dyn_riscv`):

*   **`-static`**: Tells the linker "do not look for a dynamic linker path" and "resolve what you can now"
*   **`-fno-pic`**: Disables Position Independent Code *compiler* output. This prevents the compiler from generating indirections via the GOT (Global Offset Table). Instead, it generates absolute address references or PC-relative sequences.
*   **`-mcmodel=medany`**: (RISC-V specific) Instructs the compiler to use PC-relative instructions (`auipc`) for data access where possible. This is naturally position-independent for local data.
*   **`-mno-relax`**: (RISC-V specific) **CRITICAL**. Disables linker relaxation. Prevents the linker from optimizing code sequences to use the Global Pointer (`gp`), which is unsafe because `gp` is not properly initialized for each library in our lightweight environment.
*   **`-Wl,-q` (`--emit-relocs`)**: **THE MAGIC**. Forces the linker to leave the relocation sections (`.rela.text`, `.rela.data`) in the final binary, even though it's "static".

#### The Resulting Binary

The output is an ELF file that:
*   Looks like an executable (`ET_EXEC` or `ET_DYN` depending on partial flags)
*   Contains the full code and data
*   **Crucially**: Contains a list of every single location in the code that uses an absolute address (e.g., function pointers, global variable references)

#### The Loader (Runtime Linker)

Our kernel loader (implemented in the current `core/` ELF path) treats this binary differently than a standard Linux `ld.so`:

1.  **Memory Allocation**: It allocates memory and copies the code segments (`.text`, `.data`)
2.  **Relocation Processing**: It iterates through the preservation relocation sections (`.rela...`)
3.  **Patching**: For every `R_RISCV_64` (or architecture equivalent) relocation found:
    *   It calculates the **Load Bias** (Actual Address in RAM - Preferred Address in File)
    *   It adds this bias to the value at the relocation address
    *   *Result*: An instruction that pointed to `0x1000` in the file now points to `0x80001000` in RAM
4.  **Symbol Resolution**: If it encounters a dynamic symbol (e.g., calling a kernel function like `IExec`), it looks it up in the symbol table and patches the jump target directly

#### Trade-offs and Limitations

| Feature | Impact | Analysis |
| :--- | :--- | :--- |
| **Toolchain Indep.** | ✅ Positive | Works with almost any standard GCC without libc support |
| **Performance** | 🟡 Neutral | Loading takes slightly longer (more patch points), but execution is fast (direct jumps, no GOT indirection) |
| **Strip Support** | ❌ Negative | **CANNOT STRIP BINARY**. The loader needs the symbol table (`.symtab`) to perform relocations. If you strip debug symbols, the library will crash. |
| **Code Sharing** | ❌ Negative | The code segment is modified (patched) at load time. It cannot be shared read-only between multiple processes (unlike `.so` on Linux). Fine for Exec64's single-address-space model. |

## CHAPTER 2: Memory Map (RISC-V)

Exec64 utilizza un layout di memoria virtuale a 64-bit tramite MMU Sv39 per isolare i dispositivi hardware e fornire protezione al kernel.

### Virtual Address Space Layout (RISC-V)

L'indirizzamento è basato su un **Identity Map**: l'indirizzo virtuale corrisponde all'indirizzo fisico per semplificare DMA (VirtIO) e multitasking.

| Region Name | Virtual Address | Physical Address | Size | Description |
| :--- | :--- | :--- | :--- | :--- |
| **CLINT** | `0x02000000` | `0x02000000` | ~ | Core Local Interruptor (Timer) |
| **PLIC** | `0x0C000000` | `0x0C000000` | ~ | Platform Level Interrupt Controller |
| **VirtIO** | `0x10001000` | `0x10001000` | 4KB | VirtIO MMIO Device 0 |
| **UART0** | `0x10000000` | `0x10000000` | ~ | NS16550A Serial |
| **RAM Start** | `0x80000000` | `0x80000000` | 2GB+ | System RAM (DRAM) |
| **Kernel Load** | `0x80200000` | `0x80200000` | ~ | OpenSBI jumps here |
| **Code Heap** | `0x81000000` | `0x81000000` | 252MB | Memoria eseguibile per librerie dinamiche |
| **Data Heap** | `0x90C00000` | `0x90C00000` | ~768MB | Memoria non eseguibile |

### Stack Layout

*   **Kernel Stack**: Ogni core (Hart) ha uno stack dedicato impostato in `boot.S`.
*   **Task Stacks**: Allocati dinamicamente nello heap utente (`PTE_U` attivo).
    *   Dimensione standard: 16KB.
    *   **Stack Poisoning**: L'indirizzo di ritorno iniziale è `0x0` per forzare una trap all'uscita dal `main()`.

---

## CHAPTER 3: Build Commands & Procedures

### Prerequisites

- **RISC-V Cross-Compiler**: `riscv64-elf-gcc`
- **QEMU**: `qemu-system-riscv64`
- **Make**: GNU Make 3.8+

### Quick Build Commands

> [!IMPORTANT]
> **Sempre eseguire `make clean`** prima di una compilazione completa per garantire coerenza degli oggetti.

### 1. RISC-V 64-bit (QEMU / Mars)

Target standard per lo sviluppo del kernel e delle librerie.

```bash
# Compilazione completa
make riscv

# Esecuzione in modalità testuale
./run_riscv.sh

# Esecuzione con GUI (Intuition + VirtIO-GPU)
./run_riscv_gui.sh
```

### 2. Deep Clean & Parallel builds

```bash
# Pulizia totale
make clean

# Compilazione parallela (sostituisci 8 con il numero di core)
make riscv -j8
```

---

## Related Documentation

- [01_architecture.md](01_architecture.md) - System architecture overview
- [02_file_structure.md](02_file_structure.md) - Detailed file organization
- [04_riscv_architecture.md](04_riscv_architecture.md) - RISC-V Privilege levels
- [06_riscv_hardware_setup.md](06_riscv_hardware_setup.md) - Milk-V Mars setup
