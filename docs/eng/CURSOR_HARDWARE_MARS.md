# Hardware cursor on the Milk-V Mars (DC8200)

**Status**: working and verified on real hardware.
**Date**: 2026-10-08.
**Scope**: using the Mars display controller's cursor plane from a bare-metal
OS, with no DRM, no compositor and no graphics library in between.

This is not a translation of documentation: it is what we had to find out to
make it work, with the values read from the board and the traps you fall into.

---

## 1. Why it is worth it

The hardware cursor is a plane of the display controller, **independent of the
primary plane** the OS draws on. It moves by writing its position into a
register: it costs **not one drawn pixel** — no save-and-restore of the
background, no redraw.

On the Mars that is not a luxury. The framebuffer is written through the
**uncached** alias, so every drawn pixel costs a memory write: a software
cursor would pay on every movement, a hardware one pays nothing. It is the
difference between a smooth pointer and a stuttering one.

---

## 2. What the controller offers

| | |
|---|---|
| planes | two (`CURSOR_PLANE_0`, `CURSOR_PLANE_1`), one per panel |
| format | **ARGB8888** — full colour with transparency, not monochrome |
| size | 32×32 or 64×64 |
| stacking | above everything (z = 255) |
| scaling | none |

The cursor plane is **full colour**: it is not the two-colour cursor of other
eras. The image carries its own colour and its own transparency.

---

## 3. Register map

Display controller base: `DC = 0x29400000`. All offsets are from there.

| Offset | Name | Purpose |
|---:|---|---|
| `0x1468` | `DC_CURSOR_CONFIG` | enable, size, hot spot |
| `0x146C` | `DC_CURSOR_ADDRESS` | **physical** address of the image |
| `0x1470` | `DC_CURSOR_LOCATION` | `x` in bits 0-15, `y` in bits 16-31 |
| `0x1474` | `DC_CURSOR_BACKGROUND` | background colour (legacy) |
| `0x1478` | `DC_CURSOR_FOREGROUND` | foreground colour (legacy) |
| `0x1484` | `DC_CURSOR_CLK_GATING` | defined by the driver, **never used**: not needed here |
| `0x1080` | `DC_CURSOR_OFFSET` | added for the second cursor |
| `0x24E8` | `DC_CURSOR_CONFIG_EX` | defined by the driver, never used |

### 3.1 The bits of `DC_CURSOR_CONFIG`

| Bit | Meaning |
|---|---|
| 0-4 | enable field: written as `0x0E` (bits 1, 2 and 3) |
| 5-7 | size: **0 = 32×32, 1 = 64×64** |
| 8-15 | `hot_y` |
| 16-23 | `hot_x` |

The size encoding is not documented in the driver: it is declared in the
header as an enumeration (`CURSOR_SIZE_32X32 = 0`, `CURSOR_SIZE_64X64`). The
low bits have no name and we did not interpret them: we copy what the driver
writes.

---

## 4. The sequence, and the first trap

Turning it on:

```
DC+0x1474 = 0x00000000        background colour
DC+0x1478 = 0x00FFFFFF        foreground colour
DC+0x146C = physical address of the image
DC+0x1470 = x | (y << 16)
DC+0x1468 = (1 << 5) | 0x0E   size 64x64, enabled
```

**The trap: writing the position alone has no effect at all.**

It is the first mistake we made, and it looks like anything but a sequencing
mistake: you give the coordinates and the pointer does not move; you send any
other command that rewrites the configuration, and the pointer jumps to the
coordinates you had given earlier. The cursor block reloads its working copies
when `DC_CURSOR_CONFIG` is rewritten, so **every movement must rewrite address,
position and configuration together** — which is exactly what the reference
driver does on every commit, and which we had "optimised" down to the position
alone.

Practical consequence: moving the pointer costs three register writes, not
one. That is irrelevant, and not worth trying to save.

---

## 5. The image, and where to put it

You need an **ARGB8888** buffer of 64×64 (16 KiB) — always take the largest
size, even when using the 32×32 mode, so the look does not depend on the size
field. The drawing sits in the top-left and the pointer's tip is pixel `(0,0)`,
which is also the hot spot (`hot_x = hot_y = 0`).

### 5.1 It must live in uncached memory

The controller reads the image by DMA. On the Mars the only region with the
**uncached alias** is the framebuffer reserve (`0x70000000` physical,
`0x470000000` for the CPU), so the image has to go there: straight after the
visible screen, aligned to a page.

| Resolution | Visible screen | Room for the cursor |
|---|---:|---|
| 640×480 | 1,228,800 bytes | yes |
| 1280×720 | 3,686,400 bytes | yes, 4.6 MB spare |
| 1920×1080 | 8,294,400 bytes | **no: the reserve is exactly full** |

At 1080p the framebuffer reserve has not one free byte, so the hardware cursor
has nowhere to put the image. Two ways out: ask the Core for a second uncached
window, or fall back to a software cursor at that resolution. It has to be
decided up front, not at runtime.

The problem is not only about the cursor: it applies to any DMA buffer, and it
is analysed in full in [UNCACHED_MEMORY_MARS.md](UNCACHED_MEMORY_MARS.md),
together with the possible paths and what each project needs.

### 5.2 Watch out for resolution changes

The image sits **straight after** the visible screen: if the resolution changes
and the image is not relocated, its address ends up **inside** the visible
pixels and the controller reads a piece of the console as the cursor. The
symptom is a **black square** moving instead of the pointer. It happened when
switching to 1080p. The placement must be redone on every mode change.

### 5.3 Colours, so as not to depend on transparency mode

- transparent pixels are **plain zero** (`0x00000000`), which is also the value
  written to `DC_CURSOR_BACKGROUND`;
- the outline is a **very dark gray**, not pure black.

The reason is the same in both cases: we do not know for certain whether the
controller uses the alpha channel or a **colour key** taken from the
background register. If it used the key, pure black would coincide with
transparent and the image would lose its outline; and transparent pixels at
zero work either way.

---

## 6. The second trap: hiding the pointer

`DC_CURSOR_CONFIG`, on this board, **does not turn the cursor off**: neither
the reference driver's write reproduced literally (clearing two bits and
setting one) nor clearing the **whole low field** does it. Both tried.

Worse: clearing the low field **also stops the mechanism that applies the
position**. The cursor stays frozen on screen where it was, and can no longer
be moved. The symptom is nasty — the command says "off" and the pointer is
still there — and it is what happened to us the first time.

What works, and costs **a single write**:

1. the configuration is **always** written in the "enabled" form, even while
   the pointer is off — rewriting the configuration is what makes the position
   apply, so it must not be touched;
2. to hide it, the **position** is moved off screen (`0xFFFF`).

The software keeps the real position, so switching the pointer back on brings
it back where it was, with no jump. Turning it on is one register sequence;
turning it off is three writes.

---

## 7. What has been verified

All on hardware, not inferred:

- the cursor plane is alive and visible;
- the image is read **from the right place**: the framebuffer reserve with its
  uncached alias does its job;
- the **coordinate mapping is direct**: moving the pointer to known points,
  marked by crosshairs drawn on the framebuffer, the tip lands exactly on the
  requested point, in all four corners tested. No axis swap, no scaling;
- the drawing's tip **is** the selected point;
- size field `1` (64×64) is the right one;
- clock gating **is not needed**: we never touched it;
- the position applies when the configuration is rewritten, not on its own.
- the configuration register **does not turn the cursor off**: hiding it takes
  a transparent image (§6).

---

## 8. How to test it

The HomeOS commands, all live:

```text
cursor                  status: on, position, registers, image address
cursor X Y              move the pointer to a point
cursor on | off         switch it on and off
cursor test             walk it inside the screen border
cursor probe            four crosshairs at known points, to check the mapping
cursor shape <0..2>     pointer shape
```

The pointer is also driven by the **keyboard arrow keys**, eight pixels per
press, with auto-repeat: hold two arrows and it moves diagonally. That exists
to develop windows before a mouse is available.

---

## 9. What is not done yet

- it is **not enabled at boot**, by choice: it comes alive on first use, when
  graphics start to be needed;
- there is no **click** yet;
- at 1080p there is no room for the image (§5.1);
- there is no **software** cursor, so on configurations without room there is
  no fallback yet.
