# HomeOS

Base di partenza per un **nuovo OS minimale** che gira sopra **Exec64Core** sulla
Milk-V Mars (StarFive JH7110), pensato per una shell visualizzata sull'HDMI e per
la domotica via GPIO.

Questa cartella è **autonoma** e **aggiuntiva**: è stata creata accanto al
progetto `EXEC64_multiarch` senza modificare, spostare o cancellare alcun file
esistente. È materiale "for the developer": binario, SDK di header e
documentazione. Non contiene compilatori, tool di build né sorgenti di Exec64 OS.

## Cosa contiene

```text
HomeOS/
├── README.md                      questo file
├── CORE/
│   ├── Exec64Core                 binario del Core per Milk-V Mars (RISC-V)
│   ├── SHA256SUMS                 hash di riferimento del binario
│   ├── README.md                  provenienza, avvio su Mars, sostituzione OS
│   └── boot-config/               uEnv.txt ed extlinux di esempio per la microSD
├── SDK/
    ├── README.md                  indice dell'SDK e flag di include
    ├── include/                   header del solo Exec64Core (UAPI + Exec)
    │   ├── exec64/                Init ABI, Boot Resource, BootInfo/Bundle, Task, MarsFb
    │   ├── exec/                  ExecBase, liste, memoria, syscall, moduli, device
    │   ├── devices/               ABI device display/input
    │   ├── interfaces/            superficie GPIO (wiringPi-style)
    │   └── arch/riscv64/          binding spinlock RISC-V
    └── docs/
        ├── INDEX.md               indice e provenienza della documentazione
        ├── core/                  contratti Core (boot, Init, risorse, task, syscall)
        ├── hardware/              Mars: setup, HDMI/framebuffer, input
        └── homeos/                guide scritte per HomeOS (leggi prima queste)
├── OS/                            l'OS vero e proprio (supervisor, monolitico)
│   ├── init.c                     entry _start, validazione del contesto
│   ├── boot.c                     sequenza di avvio
│   ├── display.c                  driver DC8200 / HDMI, 640x480 XRGB8888
│   ├── fbcon.c                    console di testo sul framebuffer
│   ├── font8x16.c, font_topaz.c   font (il Topaz e' MIT, Amiga-style)
│   ├── gfx.c, splash.c            primitive grafiche e schermata di avvio
│   ├── console.c, services.c      seriale e servizi diretti del Core
│   ├── usb/                       tastiera USB: STG, PHY, xHCI, HID
│   ├── shell/                     shell + un file per comando in cmds/
│   └── sys/HomeOS-SYS.exfat       immagine SYS (contiene il profilo)
├── docs/                          documentazione scritta da noi
│   ├── ita/                       italiano
│   ├── eng/                       English
│   ├── USB_KEYBOARD_MARS.md       bring-up tastiera USB: valori e trappole
│   └── CORE_NOTES_FROM_HOMEOS.md  limiti del Core emersi da un altro OS
└── tools/
    ├── mkbundle.py                bundler: Init.elf -> HomeOS.img
    ├── mksys.sh                   genera l'immagine SYS exFAT
    └── gpt_fit.py                 riallinea la GPT dopo un clone parziale
```

## Da dove iniziare

1. `SDK/docs/homeos/TOOLCHAIN.md` — toolchain necessaria su **macOS e Linux**
   (Windows escluso), installazione e verifica.
2. `SDK/docs/homeos/BOOT_CHAIN.md` — la catena di boot completa e **chi decide
   il nome dei file** (spoiler: il bootloader, non il Core).
3. `SDK/docs/homeos/GETTING_STARTED.md` — come un OS si aggancia al Core
   (entry point `_start`, `Exec64InitContext`, profili, syscall, memoria).
4. `SDK/docs/homeos/BOOT_IMAGE_FORMAT.md` — come si compone l'immagine di boot
   (`HomeOS.img`) che il Core carica.
5. `SDK/docs/homeos/DISPLAY_AND_GPIO.md` — cosa è realmente disponibile oggi per
   HDMI/framebuffer e GPIO, e cosa va implementato.
6. `SDK/docs/core/` e `SDK/docs/hardware/` — i contratti completi e il bring-up
   hardware già validato.

## Build rapida dello scheletro

```sh
cd HomeOS/OS && make        # -> Build/HomeOS.elf + Build/HomeOS.img
```

`HomeOS.img` e' il bundle pronto per la partizione `MARS_BOOT` (vedi
`CORE/boot-config/extlinux/extlinux.conf.homeos`).

## Stato e limiti (onestà tecnica)

**Funziona, verificato su hardware reale:**

- **Avvio** come payload `Init` in profilo *supervisor* (S-Mode). Richiede nel
  bundle un'immagine SYS exFAT con `Prefs/default.prefs` =
  `system-profile=performance`: senza quella il Core parte in U-Mode.
- **Display HDMI** XRGB8888 con driver proprio: dominio VOUT, clock, reset,
  routing, PHY TMDS, timing, primary plane. 640×480 e 1920×1080p60 sono
  **verificate sulla scheda**; **1280×720p60** è derivata dalla tabella PLL del
  driver Linux (i due modi verificati corrispondono a quella tabella byte per
  byte) ed è la **default all'avvio**. Il controller legge da `0x70000000`, la
  CPU scrive sull'alias non cachato `0x470000000`. Il comando `mode` elenca e
  cambia risoluzione a runtime.
- **Console di testo sul framebuffer** con font Topaz 8×16 (licenza MIT),
  cursore, scroll e area di cronologia.
- **Shell** con comandi in file separati (`OS/shell/cmds/`), input da seriale
  **e** tastiera.
- **Tastiera USB HID**: catena completa STG → VBUS → PHY → over-current →
  porta → xHCI → enumerazione → boot protocol. Si scrive e si legge sull'HDMI
  senza seriale. Dettagli e trappole in `docs/ita/USB_KEYBOARD_MARS.md`
  (versione inglese in `docs/eng/`).

**Non c'è ancora:**

- **GPIO / domotica** — è l'obiettivo originale ancora aperto. `interfaces/gpio.h`
  definisce una superficie wiringPi-style ma senza implementazione: serve un
  driver SYS_GPIO JH7110 (mappa registri dal datasheet StarFive);
- **hot-plug USB**: la porta si inizializza solo all'avvio, senza gestione del
  `Port Status Change Event`;
- **USB 3.0** (porte blu), **hub**, **mouse**, e keymap oltre la US.

**Note per chi lavora sul Core**: `docs/ita/CORE_NOTES_FROM_HOMEOS.md` raccoglie i
limiti che un OS estraneo a Exec64 OS fa emergere — syscall inutilizzabili in
S-Mode, `RAM_SIZE` fisso a 4 GiB, heap da 512 MB adiacente alla riserva
framebuffer, e il percorso del profilo che passa da un file su exFAT.

## Provenienza

Estratto da `EXEC64_multiarch` (branch `marsfb-jh7110`) in data 2026-10-06.
`CORE/SHA256SUMS` fissa l'hash del binario di riferimento; se rigeneri il Core
dal progetto originale, aggiorna quel file.
