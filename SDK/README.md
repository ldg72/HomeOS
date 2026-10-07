# HomeOS SDK — solo Exec64Core

Header e documentazione per scrivere un OS che il Core avvia come `Init`. Non
contiene codice di Exec64 OS, driver, tool di build o compilatori.

## Layout degli header

```text
SDK/
├── include/
│   ├── exec64/     UAPI del Core + ABI estese
│   │               init.h, bootresource.h, bootinfo.h, bootbundle.h,
│   │               bootcatalog.h, task.h, marsfb.h, marsfb_memory.h
│   ├── exec/       interfaccia Exec: exec_base.h, exec_lib.h, exec.h,
│   │               memory.h, syscalls.h, arch_syscall.h, module.h,
│   │               devices.h, utility.h, spinlock.h
│   ├── devices/    ABI device: display.h, marsdisplay.h, input.h
│   ├── interfaces/ gpio.h (superficie wiringPi-style)
│   ├── *.h         header C generici (stdint, stddef, string, stdlib, ...)
│   └── exec64_syscall.h   wrapper legacy dei numeri syscall
└── arch/riscv64/include/exec/spinlock.h   binding spinlock RISC-V
```

I file sono copie **non modificate** degli header del progetto Core; il layout
rispetta gli `#include` relativi (in particolare `exec/spinlock.h` →
`arch/riscv64/include/exec/spinlock.h`).

## Flag di include consigliati

```sh
-I HomeOS/SDK/include -I HomeOS/SDK/arch/riscv64/include -DARCH_RISCV64
```

Toolchain di riferimento (quella usata dall'SDK originale):

```sh
riscv64-elf-gcc -march=rv64imafdc_zifencei -mabi=lp64d -mcmodel=medany \
  -ffreestanding -nostdlib -O2 -g
```

## Documentazione

Parti da `docs/homeos/` (guide HomeOS), poi `docs/core/` (contratti del Core) e
`docs/hardware/` (bring-up Mars). Indice completo in `docs/INDEX.md`.
