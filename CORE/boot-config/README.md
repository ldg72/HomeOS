# Boot config (Milk-V Mars)

Configurazioni di riferimento per avviare il Core e HomeOS dalla partizione
`MARS_BOOT` (FAT32). Vedi `../../SDK/docs/homeos/BOOT_CHAIN.md` per la catena
completa.

## File

| File | Nota |
|---|---|
| `uEnv.txt` | variabili U-Boot; `initrd_high=...` evita che l'initrd venga spostato |
| `extlinux/extlinux.conf` | esempio originale (Debian): **non** adatto a HomeOS |
| `extlinux/extlinux_ex.conf` | esempio minimale senza riga `initrd` |
| `extlinux/extlinux.conf.homeos` | **esempio per HomeOS** con `initrd /HomeOS.img` |

## Label minima per HomeOS

```text
label homeos
    menu label HomeOS (Exec64Core)
    linux /Exec64
    initrd /HomeOS.img
    fdtdir /dtbs
    append console=ttyS0,115200 earlycon
```

Sono **due righe** a contare: `linux` (il Core) e `initrd` (il bundle OS). I
nomi sono arbitrari: il Core riceve solo un intervallo di memoria dal Device
Tree, non un nome file.

## Sulla partizione

```text
Exec64                 il binario del Core
HomeOS.img             il bundle OS (Init + eventuale filesystem)
uEnv.txt
extlinux/extlinux.conf
```
