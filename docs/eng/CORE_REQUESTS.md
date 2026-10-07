# Exec64 Core gaps — notes for kernel developers

**Date**: 2026-10-06
**Author**: the HomeOS project
**How this was gathered**: by reading the `EXEC64_multiarch` repository
**read-only** and verifying the behaviour on real Milk-V Mars hardware.
HomeOS does not modify the Core and does not use Exec64 OS: it is a foreign OS
that the Core loads as the `Init` payload in supervisor mode.

This document lists **what the Core is missing**, as seen from the outside,
with the evidence in the code and a concrete request. Observations that are not
about storage live in [CORE_NOTES_FROM_HOMEOS.md](CORE_NOTES_FROM_HOMEOS.md).

---

## Summary

| # | Gap | Impact on a foreign OS | Request |
|---:|---|---|---|
| 1 | No storage driver for Mars | saving a single byte means writing an SDHCI driver plus JH7110 clocks and pinctrl from scratch | a real block backend, **or** an explicit statement in the contract that the platform has no storage |
| 2 | `WRITABLE` on a `MEMORY_BACKED` backend | the write succeeds, is consistent, and does not persist: an OS believes it saved | document the volatility, or separate the flag from the property of the medium |
| 3 | File writing is complete only in the legacy DOS, not in the Boot Resource | two paths to the same volume with different completeness | close the gate already in the roadmap, or declare the Boot Resource block-only |
| 4 | `SysBase->ex_IDos` is the only way to read files from the bundle, and it is unversioned | a foreign OS attaches to an internal layout with no version and no magic | a versioned read capability in the Init services |
| 5 | The Init profile comes from `SYS:Prefs/default.prefs` | a foreign OS must ship an exFAT volume for a 30-byte file | move the policy into the bundle |
| 6 | The legacy exFAT writer is declared fragile by the Core itself | anyone storing persistent data on it risks silent corruption | clarify the bug status, or fix it |

---

## 1. On Mars there is no path to the physical medium

The Core says so itself:

```
[KERNEL] Milk-V Mars: Hardware storage disabled. Searching for Initrd...
```

(`core/riscv64/kernel/kernel.c:998`)

The only block device in the whole Core is the **ramdisk**, i.e. memory:
`core/riscv64/kernel/dos/handlers/ramdisk_device.c`. There is no SDHCI, MMC or
other storage driver file — verified by searching all of `core/`.

**Consequence for a foreign OS**: persistence is not something "to hook up",
it is something **to write**. Anyone wanting to save data to the microSD must
implement the JH7110 DesignWare MSHC controller, its clocks, its pinctrl and
the SD card initialisation sequence, inside their own OS. That is a job
measured in weeks, not hours.

**Request**: either a real block backend behind the Boot Resource (see §3), or
one explicit line in the boot contract: *on this platform the Core provides no
storage; persistence is the OS's responsibility*. The second option costs
nothing and removes all ambiguity.

---

## 2. `WRITABLE` + `MEMORY_BACKED`: writing does not mean saving

How the "disk" arrives today, in the order it happens:

1. U-Boot loads the bundle as initrd; the Core reads its range from the device
   tree (`linux,initrd-start` / `linux,initrd-end`).
2. The bundle contains an exFAT image (`bb_SystemOffset`).
3. `ExecBootBundleDescribe` publishes the descriptor and — when there is a
   system image — turns on **two flags unconditionally**:

   ```c
   info->bi_Flags |= EXEC64_BOOTINFO_HAS_SYSTEM_IMAGE |
                     EXEC64_BOOTINFO_SYSTEM_IMAGE_WRITABLE;
   ```

   (`core/riscv64/kernel/bootbundle.c:157`)
4. `ExecBootResourceGrant` translates that flag into a property of the
   resource:

   ```c
   BootResource.backend = BOOT_RESOURCE_BACKEND_MEMORY;
   BootResource.memory_base = (uintptr_t)boot_info->bi_SystemImageStart;
   BootResource.flags = READABLE | MEMORY_BACKED;
   if (boot_info->bi_Flags & EXEC64_BOOTINFO_SYSTEM_IMAGE_WRITABLE)
       BootResource.flags |= WRITABLE;
   ```

   (`core/riscv64/kernel/bootresource.c:66`)
5. The MMU maps the region writable (`mmu.c:291`), and Init receives a handle
   with `READABLE | WRITABLE | MEMORY_BACKED`.

**The point**: `WRITABLE` here does not describe the medium, it describes **the
RAM copy**. The flag never changes based on whether a physical medium exists:
on Mars it is always on, because the bundle always contains an image.

The `MEMORY_BACKED` flag does exist and it tells the truth, but:

- `docs/design/exec64_boot_resource_abi.md` describes it as *"backing provided
  by the boot bundle in RAM"* without saying that writes are **never
  propagated anywhere**, and without saying that no flush operation exists;
- the `WRITABLE | MEMORY_BACKED` combination is not explained: an OS reading it
  will naturally interpret it as "I can write, therefore I can save".

**Request**: one line in the contract and in the header comment —
`MEMORY_BACKED` implies **volatility**: writes stay in the RAM copy and are
lost at power-off; there is no flush. Alternatively, a dedicated flag
(`..._VOLATILE`) or an explicit *backing* field, so the OS can decide without
interpreting.

---

## 3. Two write paths, with different completeness

Today **two** implementations write to the same volume, and they are not at the
same stage.

**Path A — legacy DOS (complete, but on RAM).** `exfat.h:124-127` declares
`exfat_create_directory_entry`, `exfat_create_directory`, `exfat_write_file`
and `exfat_delete_file`; the implementations are in `exfat_handler.c:91`,
`:240`, `:376`, `:485`, with cluster allocation and freeing. All I/O goes
through:

```c
#define SYS_READ_SECTOR(s, b)  ramdisk_read_sync(s, b)
#define SYS_WRITE_SECTOR(s, b) ramdisk_write_sync(s, b)
```

(`exfat_handler.c:14-17`)

So: the path is functionally complete, it really creates files, it updates the
FAT and the allocation bitmap — **in the RAM copy**. The DOS layer above builds
`MODE_NEWFILE` (`io.c:47`) and the flush-on-close (`io.c:71`) on top of it.

**Path B — Boot Resource (partial).** The OS-side ABI has migrated only
`MakeDir`; the rest is declared pending by the project itself:

> *"Since checkpoint rev 0.124, `MakeDir` also uses the write capability […]
> File write and delete are not yet migrated."*
> — `docs/design/exec64_boot_resource_abi.md:§6`
>
> *"[ ] OS-side write path: `MakeDir` validated on QEMU; file write/delete
> pending"* — same document, §7.

**Why it matters**: a foreign OS that wants to save a file has two paths in
front of it, and the more complete one is the one the project considers legacy
and transitional. The "right" path, the versioned one, is the incomplete one.

**Request**: close the gate already planned in the roadmap, with the same care
`MakeDir` has (write authorised only if the backend declares `WRITABLE`). If
instead the design decision is that the Boot Resource stays **block-only** and
the filesystem is the OS's responsibility, write it in the contract: that is a
legitimate answer and it saves us from building on an uphill road.

---

## 4. The only way to read files from the bundle is not an ABI

An OS in S-Mode has no syscalls (§1 of `CORE_NOTES_FROM_HOMEOS.md`). To read a
file from the boot volume the only route is to attach to:

```c
struct Interface *ex_IDos;   /* exec_base.h */
```

populated by `dos_init()` (`core/riscv64/kernel/dos/library.c:125`) and defined
as a structure in `sdk/include/dos/dos.h:173` — with `Open`, `Read`,
`ReadEntries`, `ResolvePath`, and so on.

**The problem**: that structure has no magic, no version, no `StructSize`. It
is an internal layout. If a future revision changes the field order, a foreign
OS will not notice: it calls the wrong pointer and traps, with no useful
diagnostic. This is exactly the kind of dependency the rest of the boot
contract carefully avoids (magic, ABI major/minor, `StructSize`, deterministic
validation).

**Request**: either publish `DosInterface` as a versioned interface (magic +
version + `StructSize`, like `Exec64InitContext`), or offer a minimal,
versioned "read this file from the bundle" capability in the Init services.
The full DOS model is not needed: what is needed is a stable way to read one's
own startup files.

---

## 5. The profile policy lives inside an OS's filesystem

The Init profile (`protect` / `performance`) is not chosen by the boot catalog:
the Core mounts the exFAT image and reads `SYS:Prefs/default.prefs`. If the file
is missing, the default is `protect`, and a payload expecting the supervisor
receives a U-Mode context instead.

Booting a **completely foreign** OS therefore requires shipping an exFAT volume
containing that file: in our bundle that is **2 MiB of exFAT for 30 bytes of
text**. On top of that, `SYS:Prefs/...` is a reference to the Amiga/Exec64 OS
model inside the Core, while the contract itself states that the policy belongs
to the bundle.

**Request**: move the profile choice into the bundle (an header field, as long
as the ABI stays compatible) or into `BootInfo`. This is already work in
progress on your side: this note exists to say why it is felt from the outside.

---

## 6. The legacy exFAT writer is declared fragile by the Core itself

In `core/riscv64/kernel/kernel.c:1007` there is a comment that teaches a lot:

```c
// NOTE: exfat_write_file removed — writing on EVERY boot corrupts the exFAT
// cluster allocation and overwrites virtiogpu.device/input.device on the disk.
```

Reading this from the outside leads to two conclusions:

1. the legacy writer has **known allocation bugs** capable of corrupting the
   volume;
2. there was (or is) a case where writing on every boot destroyed other
   people's files.

**Why it matters to us**: the shortest road to persistence would be to lean on
that writer and then copy the image to some medium. But if the writer silently
corrupts the allocation, the corruption becomes **persistent**: far worse than
losing it on every reboot. From the outside we cannot tell whether the bug was
fixed, whether it only affects repeated use, or whether it is still there.

**Request**: a status note in the code or the documentation. "Do not use for
data that matters" is information as valuable as a fix.

---

## What we are not asking for

We are not asking the Core to become a filesystem policy layer. The current
boundary — *the Core does not interpret directories, FAT or files* — is
correct, and it is the reason a foreign OS can exist without adopting the Amiga
model.

Every request in this document lives **inside** that boundary: a real block
backend, flags that tell the truth, a versioned interface for reading one's own
files, the policy in the bundle.

---

## How to reproduce these checks

Everything stated here can be verified by reading the repository, with no
hardware:

```sh
rg -n "Hardware storage disabled" core/riscv64
rg -n "SYSTEM_IMAGE_WRITABLE"      core/riscv64
rg -n "BOOT_RESOURCE_BACKEND_MEMORY|MEMORY_BACKED" core/riscv64
rg -n "exfat_write_file|exfat_delete_file" core/riscv64/kernel/dos/handlers
rg -n "ramdisk_write_sync"         core/riscv64/kernel/dos/handlers
rg -n "MODE_NEWFILE|io_Write|io_Close" core/riscv64/kernel/dos/io
rg -n "ex_IDos" core/riscv64/kernel/dos/library.c
find core -iname "*sd*" -o -iname "*mmc*" -o -iname "*disk*"
```

The two log lines that describe the situation on the board, in a real boot:

```
[BOOTINFO] external system image candidate start=0x000000004810F000 size=0x0000000000200000; activation pending
[RAMDISK] Initializing Initrd Device...
  Source: BootInfo external exFAT image
```

---

## References

- [CORE_NOTES_FROM_HOMEOS.md](CORE_NOTES_FROM_HOMEOS.md) — the other gaps
  observed: no syscalls in S-Mode, `RAM_SIZE` fixed at 4 GiB, 512 MB heap
  adjacent to the framebuffer, misleading "U-Boot framebuffer", font typo,
  Core version not readable, SBI reset unusable.
- `docs/design/exec64_boot_resource_abi.md` — the boot resource ABI, with the
  gate list for removing the legacy path.
