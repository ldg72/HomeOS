# Uncached memory on the Milk-V Mars — the 8 MB wall

**Status**: analysis verified in the Core's and Exec64 OS's code.
**Date**: 2026-10-08.
**Scope**: any OS on the Mars that has to let a peripheral read a buffer by
DMA. HomeOS ran into it with the hardware cursor; Exec64 OS will meet it as
soon as it implements the cursor on the Mars.

---

## 1. The fact: DMA is not cache-coherent

On the JH7110 the DMA path serving the display and peripherals **is not
coherent with the CPU cache**. If the CPU writes a buffer into normal RAM and
then hands it to a peripheral, the peripheral may read stale data: the writes
are still in the cache.

This is neither a board defect nor anyone's choice: it is how it is built. It
has to be known, because it determines *where* peripheral buffers can live.

**Consequence**: every buffer a peripheral reads by DMA has to be written by
the CPU through an **uncached** path. That is not an optimisation: it is a
correctness requirement.

---

## 2. How uncached memory is obtained

The board exposes an **uncached alias** of low RAM:

```
uncached address  =  physical address  +  0x400000000
```

That is not a software convention: it is the SoC's address decoding. RAM stays
where it is, and whoever goes through that address does not meet the cache.

For an OS to use it, though, that range must be **mapped in its page tables**.
And that is where the Core comes in.

---

## 3. How much uncached memory we have

The Core maps the alias **only for the framebuffer reserve**:

| | |
|---|---|
| physical | `MARSFB_PHYS` = `0x70000000` |
| CPU alias | `MARSFB_UNCACHED` = `0x470000000` |
| size | `MARSFB_BYTES` = 1920 x 1080 x 4 = **8,294,400 bytes** |

The constants live in `exec64/marsfb_memory.h`, and **both projects use them**:
`core/riscv64/arch/riscv64/cpu/mmu.c` to map, and
`os/devices/jh7110_display/jh7110_hw.c` to draw. The window is the same for
everyone.

The reserve is sized **exactly** for 1920x1080: `8,294,400` bytes is also the
size of the framebuffer at 1080p. That is no accident, and it is the root of
all of this.

---

## 4. The wall: at 1080p there is no room for anything else

| Resolution | Visible screen | Free space in the window |
|---|---:|---:|
| 640x480 | 1,228,800 bytes | 7,065,600 |
| 1280x720 | 3,686,400 bytes | 4,608,000 |
| 1920x1080 | 8,294,400 bytes | **0** |

At 1080p the uncached window is **full**: any additional DMA buffer — the
cursor, a descriptor, anything a peripheral has to read — has nowhere to go.

### 4.1 The concrete example

The DC8200's cursor plane reads its image by DMA: it needs **16 KiB** of
uncached memory (64x64 ARGB8888 pixels). At 1080p there is none, and it cannot
be worked around by choosing the small cursor: 4 KiB does not fit into zero.

| | |
|---|---|
| 640x480 | hardware cursor: **yes** |
| 1280x720 | hardware cursor: **yes** |
| 1920x1080 | hardware cursor: **no** — it needs more uncached memory |

### 4.2 A second symptom, worth knowing

If the image stays where there is no longer room — for instance because the
resolution changed and the placement was not redone — its address ends up
**inside the visible pixels**, and the controller reads a piece of the screen
as the cursor. You see a **black square** moving instead of the pointer. It
happened when switching to 1080p with the cursor already on.

---

## 5. Who decides what

It matters not to confuse the two levels, because one can be changed and the
other cannot.

| | |
|---|---|
| **hardware** | DMA is not cache-coherent, so uncached memory is needed. It cannot be worked around. |
| **Core** | *how much* of that memory the OS may use: today exactly the framebuffer reserve. **That is a software change.** |

The controller has no preference about where to read the cursor image from:
any physical address it can reach will do. The limit is not where the
peripheral *reads*, it is where the OS manages to *write* without the cache.

---

## 6. The paths, with costs and risks

### (a) Enlarging the reserve — the smallest

64 KiB more is enough for the cursor, with some left for other uses. The only
constraint the Core declares is that the reserve stays below
`RAM_START + 1 GB`: the assert is in `jh7110_hw.c`, and the reserve currently
ends at `0x707E9000`, well before `0x80000000`.

**Verified**: in the Core's code nothing is mapped between `0x707E9000` and
`0x80000000`, and that zone is inside the RAM range the Core itself declares
(`RAM_START`, `RAM_SIZE`). We cannot prove nobody else uses it — OpenSBI, for
one, is not code we read — but no part of the Core touches it.

**Cost**: a few lines in the Core. **Risk**: low, and confined to the Core.

### (b) A second uncached window — the most general

Rather than enlarging the reserve, map another alias range elsewhere, of a
chosen size.

**Advantage**: it is not a fix for the cursor, it is a fix for *all* DMA
buffers — cursor, USB descriptors, any future peripheral. Today the only way
to get uncached memory is to fight over framebuffer space, which is the wrong
incentive.

**Cost**: a little more than (a) in the Core, plus a rule to write into the
contract: where this window is, how big it is, how it is allocated.

### (c) A software cursor at 1080p — no Core change

The pointer is drawn into the framebuffer and hidden by saving and restoring
the pixels underneath.

**Advantage**: entirely on the OS side, touching nobody.
**Cost**: it pays on every movement — writes to the uncached framebuffer, which
is exactly what the hardware cursor exists to avoid — and it brings three
complications the hardware cursor does not have: saving the pixels underneath,
redrawing itself when the console (or a window) writes over the area it covers,
and blinking without leaving trails.

### (d) The OS mapping the alias itself — not recommended

From supervisor mode the OS can write into the page tables and add a mapping
for the alias of a buffer of its own. It touches nobody's source.

**Why not**: it puts the OS inside structures that are not its own, at runtime.
If the Core rebuilds them, or walks them with different assumptions, something
breaks in a way that is hard to diagnose — and that something would be the
display, the only instrument we have to see what is happening.

---

## 7. What the two projects need

**For whoever develops the Core / Exec64 OS**

- Exec64 OS's display device uses the same constants, so **it has the same
  window**: there is a single decision and it applies to everyone;
- on the Mars that device **does not advertise cursor support yet**
  (`os/devices/jh7110_display/jh7110_device.c`: *"no cursor support advertised
  yet"*): it is unexplored ground, and the measurements in
  [CURSOR_HARDWARE_MARS.md](CURSOR_HARDWARE_MARS.md) are the starting point;
- with (a) or (b) the problem disappears for both OSes at once.

**For HomeOS**

- the code **already handles the absence**: at 1080p `cursor on` reports that
  the reserve is full instead of showing garbage, and the cursor stays off;
- when the window exists, only the placement calculation changes: no other part
  of the driver.

---

## 8. What is verified and what is not

**Verified by reading the code and measuring on the board**

- both projects use the same memory constants (`exec64/marsfb_memory.h`);
- the uncached window is 8,294,400 bytes, exactly the framebuffer at 1080p;
- at 1080p there is no room: the hardware cursor was refused by our own code,
  which computes the placement before writing the registers;
- with the image address inside the visible screen you get a black square
  instead of the pointer;
- between the end of the reserve and `0x80000000` the Core maps nothing.

**Not verified**

- that no other component (OpenSBI, firmware) uses that zone: the Core does
  not, but that is not code of ours;
- how much a software cursor at 1080p would really cost in smoothness: we have
  not written it, so we have not measured it.
