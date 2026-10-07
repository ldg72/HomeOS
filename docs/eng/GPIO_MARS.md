# GPIO on Milk-V Mars (StarFive JH7110) — bring-up notes

**Status**: driver written and building, **not yet validated on hardware**.
The self-test is ready: as soon as there is a free pin, the first command
tells us whether the register model is correct.
**Date**: 2026-10-06.

The information collected here comes from two sources, and the distinction
matters:

- **from the Linux driver** `gpio-starfive-jh7110.c` and
  `pinctrl-starfive-jh7110.c`: the register layout, the fields and their
  semantics;
- **measured on the board**: only the over-current selector, which we actually
  drove during the USB bring-up (section 3).

Everything else must be considered **derived and to be verified**.

---

## 1. The block

All registers live in the **SYS IOMUX**, base `0x13040000`. The same block
holds both pin control and pad configuration.

## 2. Register map

| Register | Address | Layout |
|---|---|---|
| `DOEN` | `0x000 + (pin & ~3)` | **6-bit** field at `8*(pin&3)` |
| `DOUT` | `0x040 + (pin & ~3)` | **7-bit** field at `8*(pin&3)` |
| `GPI` | `0x080 + (pin & ~3)` | 7-bit field at `8*(pin&3)` |
| `DIN` | `0x118` (pins 0-31) / `0x11c` (32-63) | one bit per pin |
| pin pad | `0x120 + pin*4` for pins 0..74 | see below |
| pads of pins 89-94 | `0x284 + (pin-89)*4` | same |

**Four pins per register.** The address depends on `pin & ~3`, not on the pin:
pin 25 uses the register at `+0x18`, because `25 & ~3 = 24`. The pin's field
then sits at `8 * (pin & 3)` bits. This is the least intuitive structure in
the block, and it explains why the fields look eight bits wide instead of one.

## 3. Field semantics

### `DOEN` — careful, it is inverted

| value | meaning |
|---|---|
| **0** | **output** (the pin is driven) |
| **1** | input |

It looks the wrong way round: "DOEN" suggests "output enable", yet zero is
what means the pin is driven. The Linux driver does exactly this, and the same
convention was used for the USB VBUS.

### `DOUT` — output source

| value | meaning |
|---|---|
| 0 | low level |
| 1 | high level |
| > 1 | internal source, e.g. `7` = `GPOUT_SYS_USB_DRIVE_VBUS` |

It is not a level bit: it is a **selector**. Driving a pin as plain GPIO needs
0 or 1.

### `GPI` — where the input goes

Routes the pin's signal to an internal peripheral. `0` and `1` are the low and
high constants; `2` is, for example,
`GPI_SYS_IOMUX_U0_CDN_USB_OVERCURRENT_N_IO`.

**This field is the only one measured directly**: during the USB bring-up the
over-current pin was left on the low constant, and the controller reported
over-current **with no device attached**. Moving it to the high constant made
the false alarm disappear.

### `DIN` — reading

One bit per pin, reflecting the level on the pad. For the read to work, the
**input buffer must be enabled** in the pad (bit 0 of its configuration).

### Pad configuration

| bit | meaning |
|---|---|
| 0 | enable the input buffer |
| 1-2 | drive strength (2/4/8/12 mA) |
| 3 | pull-up |
| 4 | pull-down |
| 5 | slew rate |
| 6 | schmitt trigger |

An output pin that you also want to **read back** must have bit 0 set: the
self-test needs it, and it costs nothing.

## 4. The register that does not exist

During the USB bring-up, the VBUS pin was configured with a write to
`0x130402A0` with mask `3 << 15`, taken for the pin's function register.
**That address corresponds to nothing** in this map: it would be pin 96, out
of range. The write was therefore **a silent no-op**, and the pin worked
anyway because it was already in the GPIO function.

Two things worth writing down:

- on this bus a write to a non-existent address **produces no error at all**:
  there is no way to notice except by reading the map properly;
- in the origin project's notes that register is called `FUNC` in several
  places: a misleading name, best not carried over.

## 5. The 40-pin header

Source: official Milk-V documentation. **This is not the Raspberry Pi
numbering**: physical pin 3 here is `GPIO58`, on the Pi it is `GPIO2`.
Mechanical compatibility says nothing about numbering.

| physical | GPIO | physical | GPIO |
|---:|---:|---:|---:|
| 3 | 58 | 24 | 49 |
| 5 | 57 | 26 | 56 |
| 7 | 55 | 27 | 45 |
| 8 | 5 | 28 | 40 |
| 10 | 6 | 29 | 37 |
| 11 | 42 | 31 | 39 |
| 12 | 38 | 32 | 46 |
| 13 | 43 | 33 | 59 |
| 15 | 47 | 35 | 63 |
| 16 | 54 | 36 | 36 |
| 18 | 51 | 37 | 60 |
| 19 | 52 | 38 | 61 |
| 21 | 53 | 40 | 44 |
| 22 | 50 | | |

Some pins declare an alternate function (I2C on 3-5, UART on 8-10, SPI on
19-23, LCD on 12/29/31/36). Those **without** an alternate function are the
best candidates for the first experiment: `GPIO55` (physical 7) and `GPIO44`
(physical 40).

**To be avoided**: pin 25, which is the USB VBUS, already configured by the
keyboard driver.

## 6. How to verify it without instruments

The driver exposes a self-test that needs neither multimeter nor LED:

    gpio test <pin>

The procedure: the pin is set as output, driven low and read back from `DIN`,
then driven high and read back again. On a free pin the level read **must
follow** the level driven.

If it follows, **the whole chain is correct**: the `DOEN` output field, the
`DOUT` selector, the pad and the `DIN` register. It is the honest way to
validate something derived from the driver rather than measured — and the
only way to do it without equipment.

If it does not follow, the pin is used by something or has a load: driving it
does no harm, and you try another pin.

## 7. What is missing

The current driver covers the basic use: high and low output, input, reading,
self-test. It does **not** cover:

- **pin interrupts**: the block has dedicated registers (edge or level
  detection, masks, status) starting at `0x0dc`;
- **alternate functions**: they are selected through the same `DOUT`/`GPI`
  mechanism, but using them requires the correct values for each peripheral;
- **pull-up and pull-down configurable from commands**: the bits are known,
  the command surface is missing. Two bits in an already mapped register.

## 8. References

- **Linux**: `drivers/gpio/gpio-starfive-jh7110.c` — layout and semantics of
  the `DOEN`/`DOUT`/`DIN` fields;
- **Linux**: `drivers/pinctrl/starfive/pinctrl-starfive-jh7110.c` — `SYS
  IOMUX` map, register offsets and pad layout;
- **Linux**: `include/dt-bindings/pinctrl/starfive,jh7110-pinfunc.h` — the
  values of the `GPI`/`GPOUT`/`GPOEN` selectors;
- **Milk-V**: official 40-pin header documentation.
