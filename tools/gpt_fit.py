#!/usr/bin/env python3
"""Riallinea la tabella GPT di un disco alla sua dimensione reale.

Perche' serve: quando si clona la testa di una SD su una destinazione
leggermente piu' piccola, l'header GPT primario copiato continua a descrivere
il disco *sorgente*. AlternateLBA e LastUsableLBA puntano oltre la fine della
destinazione e l'header di backup non esiste. macOS segnala la mappa come
danneggiata e puo' rifiutarsi di montare le partizioni.

Questo script riscrive i due header (primario e backup) con la geometria
corretta, lasciando **intatte** le voci di partizione: offset, GUID, nomi e
dimensioni non cambiano. Serve solo per la mappa, non tocca i filesystem.

Uso:
    sudo python3 tools/gpt_fit.py [--confirm] [--sectors N] DEVICE

Senza --confirm non scrive nulla: mostra solo cosa farebbe.
"""

import argparse
import os
import struct
import subprocess
import sys
import zlib

SECTOR = 512
GPT_SIG = b"EFI PART"
HEADER_FMT = "<8sIIIIQQQQ16sQIII"
ENTRY_SIZE = 128


def read_at(fd, offset, size):
    os.lseek(fd, offset, os.SEEK_SET)
    chunks = []
    got = 0
    while got < size:
        chunk = os.read(fd, size - got)
        if not chunk:
            break
        chunks.append(chunk)
        got += len(chunk)
    return b"".join(chunks)


def write_at(fd, offset, data):
    os.lseek(fd, offset, os.SEEK_SET)
    done = 0
    while done < len(data):
        done += os.write(fd, data[done:])


def parse_header(blob):
    (sig, rev, hsize, crc, _res, mylba, altlba, first, last,
     guid, partlba, nparts, psize, pcrc) = struct.unpack_from(HEADER_FMT, blob, 0)
    if sig != GPT_SIG:
        raise SystemExit("errore: il device non contiene un header GPT valido")
    if hsize < 92 or hsize > SECTOR:
        raise SystemExit(f"errore: header size inatteso ({hsize})")
    return dict(rev=rev, hsize=hsize, crc=crc, mylba=mylba, altlba=altlba,
                first=first, last=last, guid=guid, partlba=partlba,
                nparts=nparts, psize=psize, pcrc=pcrc)


def device_sectors(device):
    """Settori totali del device.

    Su macOS il seek a fine file sui device grezzi non e' affidabile e
    l'output di diskutil e' indentato: entrambe le cose vanno gestite.
    """
    try:
        fd = os.open(device, os.O_RDONLY)
        try:
            size = os.lseek(fd, 0, os.SEEK_END)
        finally:
            os.close(fd)
        if size > 0:
            print(f"dimensione rilevata da seek: {size} byte")
            return size // SECTOR
    except OSError:
        pass

    # diskutil preferisce il nome non-raw: /dev/rdiskN -> /dev/diskN
    node = device
    if node.startswith("/dev/rdisk"):
        node = "/dev/disk" + node[len("/dev/rdisk"):]
    try:
        out = subprocess.run(["diskutil", "info", node],
                             capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            text = line.strip()
            if text.startswith("Disk Size:"):
                if "512-Byte-Units" in text:
                    count = text.split("exactly ")[1].split(" ")[0]
                    print(f"dimensione rilevata da diskutil: {count} settori di 512 byte")
                    return int(count)
                byte_count = text.split("(")[1].split(" ")[0]
                print(f"dimensione rilevata da diskutil: {byte_count} byte")
                return int(byte_count) // SECTOR
    except Exception as error:
        print(f"(diskutil non utilizzabile: {error})")

    raise SystemExit(
        "errore: dimensione del device non rilevata.\n"
        f"  passa il numero di settori a mano, es.: --sectors 62333952")


def build_header(tpl, mylba, altlba, first, last, partlba):
    blob = bytearray(SECTOR)
    struct.pack_into(HEADER_FMT, blob, 0, GPT_SIG, tpl["rev"], tpl["hsize"], 0, 0,
                     mylba, altlba, first, last, tpl["guid"], partlba,
                     tpl["nparts"], tpl["psize"], tpl["pcrc"])
    crc = zlib.crc32(bytes(blob[:tpl["hsize"]])) & 0xFFFFFFFF
    struct.pack_into("<I", blob, 16, crc)
    return bytes(blob)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("device")
    ap.add_argument("--confirm", action="store_true", help="esegue davvero le scritture")
    ap.add_argument("--sectors", type=int, help="settori totali del device (autodetect se omesso)")
    args = ap.parse_args()

    device = args.device
    if not os.path.exists(device):
        raise SystemExit(f"errore: {device} non esiste")

    total = args.sectors or device_sectors(device)
    if total <= 34:
        raise SystemExit("errore: dimensione del device non plausibile")

    fd = os.open(device, os.O_RDONLY)
    try:
        header = read_at(fd, SECTOR, SECTOR)
        tpl = parse_header(header)
        array = read_at(fd, tpl["partlba"] * SECTOR, tpl["nparts"] * tpl["psize"])
    finally:
        os.close(fd)

    new_last_usable = total - 34
    print(f"device            : {device}")
    print(f"settori totali    : {total}  ({total * SECTOR:,} byte)")
    print(f"header copiato da : disco da {tpl['altlba'] + 1} settori")
    print(f"primo settore usa.: {tpl['first']}")
    print(f"ultimo settore usa: {tpl['last']}  ->  {new_last_usable}")

    print("\npartizioni (invariate):")
    for i in range(tpl["nparts"]):
        e = array[i * tpl["psize"]:(i + 1) * tpl["psize"]]
        if e[:16] == b"\x00" * 16:
            continue
        first_lba, last_lba = struct.unpack_from("<QQ", e, 32)
        name = e[56:128].decode("utf-16-le").split("\x00")[0]
        fits = last_lba <= new_last_usable
        print(f"  s{i+1}: LBA {first_lba}..{last_lba}  "
              f"({(last_lba - first_lba + 1) * SECTOR:,} byte)  '{name}'"
              f"{'' if fits else '   <-- non ci sta'}")
        if not fits:
            raise SystemExit("errore: la destinazione e' troppo piccola per le partizioni")

    primary = build_header(tpl, 1, total - 1, tpl["first"], new_last_usable, tpl["partlba"])
    backup = build_header(tpl, total - 1, 1, tpl["first"], new_last_usable, total - 33)

    print("\nscritture previste:")
    print(f"  header primario : offset {SECTOR}")
    print(f"  array di backup : offset {(total - 33) * SECTOR}  ({len(array)} byte)")
    print(f"  header backup   : offset {(total - 1) * SECTOR}")

    if not args.confirm:
        print("\n(nessuna scrittura: manca --confirm)")
        return

    try:
        fd = os.open(device, os.O_RDWR)
    except OSError as error:
        if error.errno == 16:  # EBUSY
            target = device.replace("/dev/rdisk", "/dev/disk")
            raise SystemExit(
                f"errore: {device} e' occupato (disco ancora montato).\n"
                f"  smontalo e riprova:\n"
                f"    sudo diskutil unmountDisk {target}\n"
                f"    sudo python3 tools/gpt_fit.py --confirm {device}")
        raise
    try:
        write_at(fd, SECTOR, primary)
        write_at(fd, (total - 33) * SECTOR, array)
        write_at(fd, (total - 1) * SECTOR, backup)
    finally:
        os.close(fd)

    fd = os.open(device, os.O_RDONLY)
    try:
        check_p = parse_header(read_at(fd, SECTOR, SECTOR))
        check_b = parse_header(read_at(fd, (total - 1) * SECTOR, SECTOR))
        check_arr = read_at(fd, (total - 33) * SECTOR, len(array))
    finally:
        os.close(fd)

    ok = (check_p["mylba"] == 1 and check_p["altlba"] == total - 1
          and check_b["mylba"] == total - 1 and check_b["altlba"] == 1
          and check_arr == array)
    print(f"\nverifica: primario MyLBA={check_p['mylba']} AltLBA={check_p['altlba']}, "
          f"backup MyLBA={check_b['mylba']} AltLBA={check_b['altlba']}, "
          f"array identico={check_arr == array}")
    print("esito:", "OK" if ok else "INCOERENTE")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
