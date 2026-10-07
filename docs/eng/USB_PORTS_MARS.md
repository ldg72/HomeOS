# The Milk-V Mars USB ports — two controllers, not one

**Date**: 2026-10-06
**Status**: reconstructed from the device tree the board **actually loads** at
boot, verified against the boot configuration on the microSD (chapter 5). This
is not an inference: it is the file the firmware hands to the Core.
**Scope**: where the four USB ports are wired, and what it takes to use them
from a bare-metal OS.

---

## 1. The fact that changes things

The Milk-V Mars exposes **four ports**, but they are not four ports of the same
controller:

| Port | Colour | Controller |
|---|---|---|
| 1 | black | the SoC's USB controller (Cadence USB3) |
| 2, 3, 4 | blue | a USB 3.0 chip **behind PCIe** |

This explains an observation that cost us time at the beginning: with the
keyboard on a blue port, `CONNECT` stayed at zero on every port of the xHCI.
It was not power: it was another controller, one we were not looking at.

---

## 2. The SoC controller (black port)

From the board's device tree:

```text
/soc/usbdrd                     compatible = "starfive,jh7110-cdns3"
  /soc/usbdrd/usb@10100000      compatible = "cdns,usb3"
      reg-names = "otg", "xhci", "dev"
      interrupts = <0x64 0x6c 0x6e>   host, peripheral, otg
      clock-names = "125m", "app", "lpm", "stb", "apb", "axi", "utmi"
      reset-names = "pwrup", "apb", "axi", "utmi"
      maximum-speed = "super-speed"
      dr_mode = "host"
      starfive,usb2-only
```

Three things are worth noting.

**The xHCI at `0x10110000` is not a controller of its own.** It is the `xhci`
*window* of the Cadence controller, which occupies three 64 KiB windows:

| Window | Base | Purpose |
|---|---|---|
| `otg` | `0x10100000` | OTG registers |
| `xhci` | `0x10110000` | host controller: the one we drive |
| `dev` | `0x10120000` | device side |

**`starfive,usb2-only`**: the SoC is configured for one USB 2.0 port. That is
why the controller reports two logical ports while the board wires up only one.
No further ports can be obtained from here.

**The electrical sequence we had measured by hand** (VBUS on SYS GPIO 25, STG
`usb0_*` clocks, inverted resets) is exactly that of the `usbdrd` node: the
`clock-names` and `reset-names` match one for one. The device tree is
therefore the reference against which to check the driver, not just a
document.

---

## 3. The three blue ports are behind PCIe

The JH7110 has **two PCIe controllers** (PLDA XpressRICH3-AXI), and on this
board both are enabled:

| | `pcie@2B000000` | `pcie@2C000000` |
|---|---|---|
| bar | `0x2B000000` | `0x2C000000` |
| config space | `0x94000000` | `0x9C000000` |
| PHY | `pcie0-phyctrl@10210000` | `pcie1-phyctrl@10220000` |
| PERST reset | GPIO 26 (`0x1a`) | GPIO 28 (`0x1c`) |
| clock-names | `noc`, `tl`, `axi_mst0`, `apb` | same |
| resets | 6: `rst_mst0`, `rst_slv0`, `rst_slv`, `rst_brg`, `rst_core`, `rst_apb` | same |

One of the two feeds the M.2 slot, the other feeds the USB 3.0 chip (a 4-port
controller, of which the board exposes three). Which is which **is not written
in the device tree**: it is discovered by enumerating the PCI bus. The same
arrangement is found on the VisionFive 2, which uses the same SoC.

**What it takes to use the blue ports**, in order:

1. enable the clocks and release the controller's 6 resets;
2. configure the PHY (`phyctrl` registers) and the `stg_syscon`;
3. assert and release PERST, wait for link training;
4. enumerate the PCI bus from config space and assign the BARs;
5. drive the xHCI of the chip you find: the xHCI driver we already wrote is
   largely reusable, because it is a standard xHCI.

Steps 1-4 are the bulk of the work: far more than a peripheral driver.

---

## 4. Practical consequence: connecting a mouse

**Short road — a USB 2.0 hub on the black port.** This requires adding hub
support to our xHCI: recognise class 9, address the hub, follow its port status
changes and address the devices downstream. Everything else (HID, keymap,
auto-repeat) is already written and validated.

**Long road — PCIe to the USB 3.0 chip.** It gives three more ports and
SuperSpeed, but requires the sequence in chapter 3. It belongs in the roadmap
as a separate thread, not as a step towards the mouse.

---

## 5. Where this data comes from

This analysis does not come from a third-party document: it comes from the
board's device tree, read with `dtc`. And that file is **exactly the one
U-Boot loads**, as can be checked on the boot partition:

```text
# uEnv.txt
fdt_addr_r=0x48000000
ramdisk_addr_r=0x48100000
fdtfile=starfive/jh7110-visionfive-v2.dtb

# extlinux/extlinux.conf
    linux /Exec64Core
    initrd /HomeOS.img
    fdtdir /dtbs
```

This also closes the loop with the Core log line
`platform-data=0x0000000048000000`: that is the address in `fdt_addr_r`, i.e.
the device tree. (The file is named `jh7110-visionfive-v2.dtb`, which is
misleading: inside, `model = "Milk-V Mars"`.)

At runtime, HomeOS re-reads the same tree with the `fdt` command — the address
comes from the Core in `SysBase->ex_PlatformData` — and that remains the best
way to check that the board and the document agree:

**Verified on the board on 2026-10-06**: the `fdt` command reads the tree
U-Boot passed to the Core.

```text
fdt          summary: size, version, model, memory
fdt usb      the controller node and its properties
fdt pcie     the two PCIe controllers, PHY, resets
fdt mmc      the two storage controllers
```

---

## 6. Found along the way: the two storage controllers

The same device tree describes storage, and this is information we need right
away for the persistent volume:

| Node | Base | Configuration | What it is |
|---|---|---|---|
| `sdio0@16010000` | `0x16010000` | 8-bit, non-removable, hs200, `cap-mmc-hw-reset` | eMMC |
| `sdio1@16020000` | `0x16020000` | 4-bit, `cap-sd-highspeed`, `no-mmc`, `broken-cd` | microSD |

Useful details: `biu`/`ciu` clocks (indices 0x5b/0x5d for sdio0, 0x5c/0x5e for
sdio1), one reset per controller (0x40 and 0x41), one interrupt each (0x4a and
0x4b). `broken-cd` means card-detect is not wired: **card presence is found by
talking to the card**, not by reading a pin.
