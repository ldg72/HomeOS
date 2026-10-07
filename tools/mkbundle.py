#!/usr/bin/env python3
"""Impacchetta un Init ELF64 RISC-V in un BootBundle per Exec64Core.

Uso:
    mkbundle.py OUTPUT.img INIT.elf [SYSTEM.exfat]

Produce un bundle v1.1 (header da 80 byte, payload allineati a 4096) con
eventuale immagine di sistema opzionale e senza catalogo. Il nome del file di
uscita e' arbitrario: lo decide la configurazione di boot (vedi
SDK/docs/homeos/BOOT_CHAIN.md).
"""

import struct
import sys

MAGIC = 0x4C444E4234365845      # "EX64BNDL"
ABI_MAJOR = 1
ABI_MINOR = 1
HEADER_SIZE = 80
ALIGNMENT = 4096
PAYLOAD_ELF64 = 1
PAYLOAD_EXFAT = 2
EM_RISCV = 243


def align_up(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def read_file(path, label):
    try:
        with open(path, "rb") as stream:
            data = stream.read()
    except OSError as error:
        raise SystemExit(f"mkbundle: cannot read {label} '{path}': {error}")
    if not data:
        raise SystemExit(f"mkbundle: {label} '{path}' is empty")
    return data


def check_elf_riscv64(init):
    if len(init) < 20 or init[:4] != b"\x7fELF":
        raise SystemExit("mkbundle: Init is not an ELF file")
    if init[4] != 2 or init[5] != 1:
        raise SystemExit("mkbundle: Init must be ELF64 little-endian")
    machine = init[18] | (init[19] << 8)
    if machine != EM_RISCV:
        raise SystemExit(f"mkbundle: Init e_machine={machine}, expected RISC-V")


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: mkbundle.py OUTPUT.img INIT.elf [SYSTEM.exfat]")

    out_path = sys.argv[1]
    init = read_file(sys.argv[2], "Init")
    check_elf_riscv64(init)

    system = read_file(sys.argv[3], "system image") if len(sys.argv) == 4 else b""

    init_offset = ALIGNMENT
    system_offset = 0
    total_size = align_up(init_offset + len(init), ALIGNMENT)
    if system:
        system_offset = total_size
        total_size = align_up(system_offset + len(system), ALIGNMENT)

    header = struct.pack(
        "<QHH I Q Q Q Q Q I I Q Q",
        MAGIC,
        ABI_MAJOR,
        ABI_MINOR,
        HEADER_SIZE,
        total_size,
        init_offset,
        len(init),
        system_offset,
        len(system),
        PAYLOAD_ELF64,
        PAYLOAD_EXFAT if system else 0,
        0,
        0,
    )
    if len(header) != HEADER_SIZE:
        raise SystemExit(f"mkbundle: internal error, header is {len(header)} bytes")

    blob = bytearray()
    blob += header
    blob += b"\x00" * (init_offset - len(blob))
    blob += init
    if system:
        blob += b"\x00" * (system_offset - len(blob))
        blob += system
    if len(blob) < total_size:
        blob += b"\x00" * (total_size - len(blob))

    try:
        with open(out_path, "wb") as stream:
            stream.write(blob)
    except OSError as error:
        raise SystemExit(f"mkbundle: cannot write '{out_path}': {error}")

    print(f"HomeOS boot bundle: {out_path}")
    print(f"  Init:   offset=0x{init_offset:x} size={len(init)}")
    if system:
        print(f"  System: offset=0x{system_offset:x} size={len(system)}")
    print(f"  Total:  {total_size} bytes")


if __name__ == "__main__":
    main()
