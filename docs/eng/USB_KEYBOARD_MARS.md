# USB Keyboard on Milk-V Mars (StarFive JH7110) — bring-up notes

**Status**: full chain working on real hardware.
**Date**: 2026-10-06.
**Scope**: bare-metal OS running directly on the hardware, with no
U-Boot/Linux underneath. Written to be reusable by any OS.

This document describes what it takes to get from "board powered on" to
"keystrokes reach the OS", with the values measured on the board and the traps
actually hit along the way. It is not a translation of existing documentation:
it is what we had to discover to make it work.

---

## 1. First fact: which port

The Milk-V Mars exposes **four physical USB ports**:

- **one black** → USB 2.0, attached to the xHCI driven by these notes;
- **three blue** → USB 3.0, a **different controller** with a different MMIO base.

If you plug the keyboard into a blue port, this procedure will never see it:
`CONNECT` stays zero on every port of the xHCI. It is the mistake that costs
the most time, because it looks like a power problem.

The USB 2.0 controller reports **2 logical ports** (`HCSPARAMS1`), 32 slots
and 31 scratchpads.

---

## 2. Block map

| Block | Base | Purpose |
|---|---|---|
| xHCI | `0x10110000` | host controller |
| USB PHY | `0x10200000` | USB 2.0 PHY |
| STG CRG | `0x10230000` | USB domain clocks and resets |
| STG SYSCON | `0x10240000` | host mode |
| SYS CRG | `0x13020000` | system clocks (`SYSCLK_USB_125M` at `+0x17C`) |
| SYS SYSCON | `0x13030000` | USB split (`+0x18`) |
| SYS GPIO | `0x13040000` | VBUS pinmux and over-current routing |

Derived from the xHCI (`CAPLENGTH = 0x80`, `DBOFF = 0x3000`, `RTSOFF = 0x2000`):

| Register | Address |
|---|---|
| Operational base | `0x10110080` |
| `PORT1SC` / `PORT2SC` | `0x10110480` / `0x10110490` (stride `0x10`) |
| Runtime interrupter 0 | `0x10112020` (`IMAN`), `+0x08` `ERSTSZ`, `+0x10` `ERSTBA`, `+0x18` `ERDP` |
| Doorbell array | `0x10113000` |

---

## 3. Electrical sequence

Must be executed **in this order**. Every step corresponds to a physical test.

### 3.1 Host mode (STG SYSCON + 0x04)

```
mask  (7<<16) | (1<<19) | (0xF<<20)
value (1<<17) | (1<<19) | (1<<20) | (1<<22) | (1<<23)
```

That is: host strap, host-mode suspend, bypass, PLL enabled, refclk.

### 3.2 USB clocks (STG CRG + id*4, id 1..6)

Write `0x80000000` (enable bit) into each. The ids are:

| id | clock |
|---|---|
| 1 | `usb0_apb` |
| 2 | `usb0_utmi_apb` |
| 3 | `usb0_axi` |
| 4 | `usb0_lpm` |
| 5 | `usb0_stb` |
| 6 | `usb0_app_125` |

### 3.3 USB resets (STG CRG + 0x74)

Clear bits **7, 8, 9, 10**. Status lives at **`+0x78`**, and the bits read **1
when the reset has been released** — inverted from what you would expect.

### 3.4 VBUS (SYS GPIO 25)

Per the Milk-V Mars device tree, `usb0_pins` uses `sysgpio` pin 25 with
`GPOUT_SYS_USB_DRIVE_VBUS`.

| Register | Address | Value |
|---|---|---|
| `DOUT` | `0x13040058` | pin 25 field (bits 8-14) = **7** |
| `DOEN` | `0x13040018` | pin 25 field (bits 8-13) = **0** |
| `FUNC` | `0x130402A0` | bits 15-16 = 0 |
| `PADCFG` | `0x13040184` | clear bits 6, 5, 4, 3, 0 |

### 3.5 USB 2.0 PHY

| Register | Address | Bit |
|---|---|---|
| `SYSCLK_USB_125M` | `0x1302017C` | bit 31 (enable) |
| `USB_PHY_CLK_MODE` | `0x10200000` | bit 1 (`RX_NORMAL_PWR`) |
| `USB_PHY_LS_KEEPALIVE` | `0x10200004` | bit 4 (`LS_KEEPALIVE`) |
| `SYS_USB_SPLIT` | `0x13030018` | bit 17 (`USB_PDRSTN_SPLIT`) |

### 3.6 Over-current — **the silent trap**

`0x13040080` routes the internal over-current sources. The selector for
`GPI_SYS_USB_OVERCURRENT` is **GPI 2**, so its field sits in **bits 22-16**
(`<< 8 * (2 % 4)`), mask `0x7F << 16`.

Values: **0 = constant low**, **1 = constant high**.

With the reset value (constant low) the controller reports **`OCA` asserted on
both ports with no device attached** — a false over-current. Symptom:
`PORTSC = 0x00100088`, `power=no`, `oca=si`.

Setting the selector to **constant high** clears `OCA` immediately. Only then
does `PORT_POWER` stay set. `OCC` remains 1 as a historical change bit: that is
normal and must not be cleared.

### 3.7 Port power and reset

Writes must use the **neutral** form: preserve the status and read/write bits,
and add only what you mean to change.

```
neutral = PORTSC & (CCS | OCA | SPEED | PLS | PP)
power:  neutral | PORT_POWER      (bit 9)
reset:  neutral | PORT_RESET      (bit 4)
```

Writing the bare bit (`0x00000200`) **does not work**: the readback stays
unchanged. The reset is complete when `PORT_RESET` reads back 0; then
`PORT_ENABLED` must become 1.

**Values observed on the board** (low-speed keyboard on port 1):

| Moment | `PORTSC` | decode |
|---|---|---|
| empty port, powered | `0x001002A0` | `pls=5` (RxDetect) |
| keyboard attached, before reset | `0x00120AE1` | `connect=1`, `pls=7` (Polling), `speed=2` (low) |
| after reset, controller halted | `0x00320A63` | `enabled=1`, `pls=3` (U3) |
| after reset, controller running | `0x00220A03` | `enabled=1`, `pls=0` (U0) |

The difference between U3 and U0 depends on the controller state at reset time.
Both are valid.

---

## 4. xHCI: minimal bootstrap

1. **Halt** (`USBCMD = 0`) and wait for `USBSTS.HCHalted`.
2. **Reset** (`USBCMD.RESET = 1`, bit 1) and wait for the bit to clear and for
   `USBSTS.CNR` to go to 0.
3. **`CONFIG`** = number of enabled slots.
4. **`DCBAAP`** = physical address of the DCBAA (64-byte aligned).
5. **Event ring**: an ERST with one entry `{address, TRB count}`, then
   `ERSTSZ = 1`, `ERSTBA`, and `ERDP` with the `EHB` bit (bit 3) set.
6. **`CRCR`** = command ring address **| 1** (bit 0 is the cycle bit).
7. **Run** (`USBCMD.RUN = 1`, bit 0), wait for `HCHalted` to clear.

Rings are arrays of 16-byte TRBs: `param` (8), `status` (4), `control` (4).
A **Link** TRB closes the ring: it must be rewritten on every wrap with the
current cycle and `TC=1`, then the cycle flips.

64-bit register pointers are written **low dword first, then high**
(xHCI requirement).

---

## 5. Enumeration and HID

Minimal sequence, after the bootstrap:

1. **Enable Slot** (TRB type 9) → the completion event carries the slot ID in
   bits 24-31 of its *control* dword.
2. **Address Device** (TRB type 11, `BSR=0`) with an input context holding the
   slot context plus the EP0 context.
3. `GET_DESCRIPTOR` device (18 bytes) → `idVendor`/`idProduct`.
4. `GET_DESCRIPTOR` config: first 9 bytes for `wTotalLength`, then the whole
   thing. Look for the interface with class 3 / subclass 1 / protocol 1
   (HID boot keyboard) and its interrupt IN endpoint.
5. `SET_CONFIGURATION`.
6. `HID SET_PROTOCOL` with value 0 (boot) — the report is then a fixed 8 bytes
   and the report descriptor is not needed.
7. `Configure Endpoint` (TRB type 12) for the interrupt IN endpoint.
8. A **Normal TRB** (type 1) of 8 bytes on that endpoint, with `IOC`, then
   ring the device doorbell with the endpoint's DCI.

### Endpoint addressing (DCI)

`DCI = 2 * endpoint_number + direction`, with direction 1 = IN.
So EP0 = 1, EP1 OUT = 2, **EP1 IN = 3**.

### Boot keyboard report (8 bytes)

| Byte | Content |
|---|---|
| 0 | modifiers: `0x02`/`0x20` = left/right shift |
| 1 | reserved |
| 2-7 | up to 6 HID keycodes held simultaneously |

For console use you only care about **newly pressed** keys: compare the report
against the previous one, otherwise you generate autorepeat.

---

## 6. The four traps

These are the things you **cannot** deduce from the documentation, and the ones
that cost us the most time. If you are writing your own driver, read this
chapter before the others.

### 6.1 Ordering: controller first, port second

`HCRST` (the controller reset) **wipes port state**. If you reset the port
*before* the controller, that reset is cancelled and `Address Device` fails
even though the port looked fine.

```
wrong:     reset port  →  reset controller  →  slot
correct:   reset controller  →  reset port  →  slot
```

Symptom: `Enable Slot` succeeds (it does not depend on the port), `Address
Device` fails. It is insidious because `PORTSC` read right after the port reset
shows `enabled=1`: everything looks fine, and it has already been undone.

### 6.2 `CErr = 3` in the EP0 context

The `CErr` (Error Count) field lives in **bits 1-2** of dword 1 of the endpoint
context. For control endpoints the specification requires the value **3**.

With `CErr = 0` the controller answers **Command Completion Code 11 (TRB Error)**
to the `Address Device` command. The rest of the context can be perfect: that
one field is enough.

Correct EP0 context for a low-speed device:

```
dword 1 = (3 << 1) | (4 << 3) | (8 << 16)
            CErr     type        MaxPacketSize
                     (Control Bidirectional)
dword 2 = (ring_address & ~0xF) | 1          <- bit 0 = DCS, must be 1
dword 3 = ring_address >> 32
dword 4 = 8                                   <- Average TRB Length
```

### 6.3 The Slot ID goes in dword **3** of the command TRB

Layout of a Command TRB:

| dword | content |
|---|---|
| 0-1 | parameter (e.g. pointer to the input context) |
| 2 | status — **zero for Address Device** |
| 3 | bit 0 cycle, bit 9 `BSR`, bits 10-15 type, **bits 24-31 slot ID** |

Putting the slot ID in dword 2 **does not fail immediately**: the controller
reads dword 3, finds zero, and zero is the reserved slot. The result is again
**code 11 (TRB Error)**.

The trap is that `Enable Slot` keeps working — there the slot ID is zero even
in the right place. The broken command does not contradict you until the first
command carrying a real slot.

### 6.4 The `Interval` field is not the `bInterval`

In the endpoint context, the `Interval` field (bits 16-23 of dword 0) **does
not hold the value read from the descriptor**: it is an exponent of a power of
two of microframes, and the period is `2^(Interval-1) × 125 µs`.

Writing a keyboard's raw `bInterval` (`10`) means asking for a poll every
**64 ms**. Symptom: the keyboard *works*, but **typing fast, or with two
hands, loses characters**, while one-finger typing does not — because slow
presses last longer than a sampling period, and fast overlaps do not.

For a `bInterval` of 10 the correct value is **3**, about 1 ms: you take the
exponent, `fls(bInterval) - 1`, not the value.

How to recognise it from the numbers: count the reports received. With the
wrong value you see roughly **15 per second** even during dense typing; with
the right one the count grows with the typing.

---

## 7. DMA and cache coherency

Facts verified on the JH7110:

- **The U74 cores do not expose Zicbom**: `cbo.clean` traps (illegal
  `MCAUSE`). Explicit cache maintenance is not an option.
- Coherency for these peripherals comes from the JH7110 **coherent front port**
  plus **barriers** (`fence`) only, before and after every hand-off to the
  controller. Verified with a real command/completion round-trip.
- **DMA memory must sit below 4 GiB**: the controller addresses 32 bits.
- Addresses handed to the controller must be **physical**. If your OS identity
  maps RAM, the allocator's pointer is fine as-is.

**Negative trap worth knowing**: this platform has an *uncached alias* of RAM
(`physical + 0x400000000`), but it is **not** a general mapping: in the
reference configuration it covers **only the 8 MB framebuffer**, page by page.
It cannot be used to make DMA buffers coherent.

---

## 8. Current limitations

What this implementation does **not** do, in the interest of honesty:

- **no hot-plug**: the port is powered and reset once. If the keyboard is not
  attached at boot you need a reboot (or to re-run the whole sequence). There
  is no `Port Status Change Event` handler redoing the bring-up;
- **no hub support**: only a device directly on a root hub port;
- **USB 2.0 only**: the blue ports (USB 3.0) belong to another controller;
- **US keymap only**: no accents, no national layouts;
- **no mouse**: boot-protocol keyboard only.

---

## 9. Quick reference card

```
correct port ............... the BLACK one (USB 2.0)
PORTSC idle ................ 0x001002A0   pls=5
PORTSC with keyboard ....... 0x00120AE1   connect=1 pls=7 speed=2
PORTSC after reset ......... 0x00220A03   enabled=1 pls=0

over-current ............... 0x13040080   bits 22-16 = 1 (constant high)
VBUS pin 25 ................ DOUT 0x13040058 = 7<<8
                             DOEN 0x13040018 = 0
PHY ........................ 0x10200000 bit1, 0x10200004 bit4
USB split .................. 0x13030018 bit17
SYSCLK_USB_125M ............ 0x1302017C bit31

xHCI op .................... 0x10110080
port 1 / port 2 ............ 0x10110480 / 0x10110490
doorbell 0 ................. 0x10113000

Command Completion ......... type 33, success = code 1, TRB Error = 11
Port Status Change ......... type 34
Transfer Event ............. type 32
```

## 10. External references

- **xHCI Specification** (Intel) rev 1.1/1.2 — TRBs, slots, contexts, rings.
  This is the source for the field layout; without it the three traps in
  chapter 6 are invisible.
- **USB 2.0** chapter 9 — descriptors and standard requests.
- **USB HID 1.11** + HID Usage Tables — the 8-byte boot report.
- **Linux**: `drivers/usb/host/xhci*.c` (`xhci-mem.c` for contexts,
  `xhci-ring.c` for TRBs), `drivers/hid/usbhid/hid-core.c`.
  Useful as a cross-check, not to copy: the kernel is GPLv2.
- **Linux**: `phy-jh7110-usb`, the StarFive clock/reset drivers, and the
  Milk-V Mars DTS for the VBUS pinmux.

The register values in this document are **measured on the board**, not
inferred: where an external reference and the board disagree, the board wins.
