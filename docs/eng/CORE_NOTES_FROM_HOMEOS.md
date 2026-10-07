# Core limitations as seen from another OS

**Date**: 2026-10-06.
**Context**: HomeOS is a minimal OS running as the Core's `Init` payload on the
Milk-V Mars, written **without** using anything from Exec64 OS: no
`dos.library`, no `graphics.library`, no device model, no interfaces, no Amiga
model.

Writing an OS that deliberately ignores that model acted as a probe: it
surfaced Core assumptions that Exec64 OS never meets, because it was built
around them. These notes are for whoever maintains the Core.

---

## 1. In supervisor mode there are no syscalls

The Core intercepts `ecall` from **U-Mode** tasks. A payload loaded in
**S-Mode** (the `performance` profile) that executes `ecall` is not talking to
the Core: it is talking to **OpenSBI**, in M-Mode. The symptom is a stream of:

```
sbi_ecall_handler: Invalid error 104 for ext=0x2 func=0x10000000
```

where `104` is the `h` the user just typed — because the return value is being
interpreted as an error code.

**Consequence**: in S-Mode you use direct calls and direct register access.
In our case:

| U-Mode (syscall) | S-Mode (direct) |
|---|---|
| `SYS_KPUTS` | `SysBase->ex_DebugPutS` |
| `SYS_KGETC` | `SysBase->ex_DebugGetC` |
| `SYS_AVAIL_MEM` | `SysBase->ex_IExec->AvailMem` |
| `SYS_GET_TICK` | `rdtime` |
| `SYS_YIELD` | `wfi` |
| `SYS_MMIO_*` | direct MMIO access |

This is not a defect: it is the definition of a supervisor OS. But it is
**documented nowhere**, and you only find out by getting it wrong. It deserves
a line in the profile contract.

---

## 2. Memory: 512 MB, and it is not the OS's choice

```c
#define RAM_START         0x40000000
#define RAM_SIZE          (4096ULL * 1024 * 1024)   /* 4GB — a constant! */
#define KERNEL_HEAP_START (RAM_START + 0x10000000)  /* 0x50000000        */
#define KERNEL_HEAP_SIZE  (512 * 1024 * 1024)       /* 512 MB            */
```

Three observations.

**`RAM_SIZE` is a compile-time constant and it says 4 GiB.** The Core *has* the
DTB — it uses it to locate the initrd range — but it never reads the `/memory`
node. On an 8 GB board the Core declares 4 GB, and RAM beyond that is not
mapped. Using it requires reading `/memory` from the device tree and mapping
accordingly.

**The heap is 512 MB, split into two 256 MB pools**: `CodeRAM` (executable) and
`DataRAM`. The split is a fixed 50/50.

**The framebuffer sits immediately after the heap**, and a `_Static_assert`
enforces it:

```
0x50000000  CodeRAM  256 MB
0x60000000  DataRAM  256 MB
0x70000000  MARSFB_PHYS — framebuffer reservation, 8 MB
```

So **the heap cannot grow without moving the display reservation** — and that
address is programmed into the DC8200 as the scanout base, so the change
cascades into every display driver, Core and OS alike.

An architecture that would avoid the problem: a small, "DMA-safe" low pool
below 4 GiB (where the heap and framebuffer live) and the rest of RAM as a
general pool mapped separately.

**Constraint not to lose sight of**: JH7110 peripherals address only the first
4 GiB. RAM above 4 GiB is fine for the kernel and applications, not for DMA
descriptors.

---

## 3. The uncached alias is not general

There is an uncached alias of RAM (`physical + 0x400000000`), but in the
current configuration it covers **only the 8 MB framebuffer**, page by page.
It cannot be used to make arbitrary DMA buffers coherent.

On this platform, coherency for USB/VirtIO comes from the coherent front port
plus barriers alone — the U74 cores do **not** expose Zicbom, so `cbo.clean`
traps. This is worth writing down because it is counter-intuitive: people
coming from other platforms look for explicit cache maintenance, and here there
is none.

---

## 4. The Init profile comes from a hidden exFAT file in the bundle

The profile (`protect` / `performance`) is **not** selected by the boot
catalog. The Core mounts the **exFAT** resource of the bundle and reads:

```
SYS:Prefs/default.prefs      →      system-profile=performance
```

If the file is absent, the default is `protect` (Init in U-Mode) and a payload
expecting supervisor mode receives a U-Mode context instead.

Two notes:

- the `SYS:Prefs/...` path is an Amiga/Exec64 OS model reference living inside
  the Core. The *policy* should belong to the bundle, as the contract itself
  states: today it is a file on an OS-specific filesystem;
- to boot a **completely unrelated** OS you must still supply an exFAT image
  containing that single file. In our case the bundle carries 2 MB of exFAT
  for a 30-byte file.

---

## 5. The "U-Boot framebuffer": a trail that leads nowhere

At boot the Core prints:

```
Exec: Milk-V Mars Detected. Activating Static U-Boot Framebuffer Override.
Exec: Found fb! Base: 0x00000000FE000000, W: 1920, H: 1080
Exec: Framebuffer filled (Stripes).
```

It looks as though a framebuffer is already available. In reality U-Boot,
before handing over, **turns the display off** (its `sf_vop_remove` routine
stops scanout, asserts resets and disables clocks and power). The stripes are
written into memory that **nobody is scanning out**: they are invisible, and
were never seen in our tests.

It is a Core-internal diagnostic path that contradicts the rest of the model
(the real reservation is at `0x70000000`, with the uncached alias at
`0x470000000`). Anyone writing an OS finds it misleading: it is the first thing
you try to use, and it fails for a reason the log does not mention.

---

## 6. The typo in the Core font

The 8×16 font inside the Core (`font_modern.c`) has the `}` glyph written as:

```c
{0x00,0x30,0x08,0x08,0x04,0x08,0x08,030,0x00,...}
                                     ^^^
```

`030` in C is an **octal literal**: it is 24 (`0x18`), not `0x30`. The
OS-owned copy of the same font (`os/libraries/graphics/font_impulse.h`) has the
correct value. Effect: in the Core console the closing brace renders wrong.

---

## 7. The Core version is not readable by the OS

The Core announces its version on the serial line:

```
   EXEC64 OS (v0.125 - 2026-08-24)
```

but it **does not expose it to Init**: there is no field in `ExecBase`, nothing
in the boot context, and `EXEC64_VERSION_STR` is a Core-internal constant.

Practical consequence: an OS that wants to show "running on <Core> version X"
must **keep that number hardcoded** and remember to update it — with no way to
notice if it is wrong. In HomeOS it lives in `OS/version.h` with a comment
saying so.

An OS has a legitimate interest in knowing what it runs on: diagnostics, error
messages, and refusing untested combinations. A field in the `Init` context
(ideally numeric major/minor) or in `SysBase` would be enough, and it would be
consistent with the rest of the boot contract, which is already versioned and
validated by the Core.

## 8. The SBI reset is not usable on this board

From a supervisor OS the only way to request a full restart is the SBI **SRST**
extension (`0x53525354`), which from S-Mode reaches OpenSBI in M-Mode. On the
Milk-V Mars it **does not work**: the call drops the video output and the board
does not come back, staying halted until power is removed.

The observation that makes the diagnosis solid: after the screen goes dark
**our fallback message does not appear**, even though it would redraw the
console. That says the CPU never got back to executing OS code — it is the
firmware that stopped, not the OS that lost its output.

Practical consequence for anyone writing an OS: **do not put the SBI reset on
an automatic or default path.** The function exists and is the architecturally
correct one, but on this firmware it must be treated as an explicit attempt,
with the knowledge that it may require physical intervention.

A *safe* restart of the session alone — clearing the console and redrawing the
boot screen — does not touch the hardware and always works. That is the
behaviour to use as the default.

## 9. What worked well

For balance, it deserves saying: the boot contract is **solid**. The bundle is
validated by magic and ABI, Init is handed a versioned context, rejection is
deterministic with a reason on the serial line, and no filename is hardcoded in
the Core. Switching from one OS to another amounted to replacing one file: no
change to the Core binary, and the serial error message was always enough to
understand what was wrong.

The supervisor profile also proved complete: `SysBase`, the services, the
framebuffer mapping and MMIO access were enough to write real drivers (display,
USB) without touching the Core.
