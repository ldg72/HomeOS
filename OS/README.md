# HomeOS — scheletro Init

Sorgente minimo di HomeOS: l'`Init` che il Core avvia. Compila in un ELF64
RISC-V e lo impacchetta in un bundle `HomeOS.img`.

## Cosa fa

1. riceve `Exec64InitContext` dal Core (in `a0`);
2. valida magic / ABI / size;
3. rileva il profilo (supervisor S-Mode oppure protect U-Mode);
4. stampa un banner sulla seriale;
5. entra in un loop di servizio, punto in cui agganciare display, input, GPIO
   e shell.

## Build

Richiede la toolchain `riscv64-elf-` (la stessa usata dal Core):
sulle piattaforme supportate (macOS e Linux) vedi `../SDK/docs/homeos/TOOLCHAIN.md`
per installazione e verifica (Windows non e' supportato).

```sh
cd HomeOS/OS
make
```

Produce:

```text
HomeOS/OS/Build/HomeOS.elf    il payload Init
HomeOS/OS/Build/HomeOS.img    il bundle di boot
```

## Avvio su Milk-V Mars

1. copia `Build/HomeOS.img` sulla partizione `MARS_BOOT`;
2. nel `extlinux.conf` assicurati di avere `initrd /HomeOS.img`
   (esempio pronto: `../CORE/boot-config/extlinux/extlinux.conf.homeos`);
3. avvia la scheda e guarda la seriale (115200 8N1).

Atteso in seriale:

```text
[HomeOS] Init avviato
[HomeOS] Init ABI: 1.x  struct_size=..
[HomeOS] profilo: protect (U-Mode)
[HomeOS] ...
[HomeOS] ingresso nel loop di servizio
```

## Nota sui profili

Se il Core avvia Init in **protect**, il contesto e' v1.1 (80 byte): niente
SysBase, niente accesso diretto a MMIO. Se lo avvia in **supervisor**
(Performance), il contesto e' v1.2 (96 byte) e sono disponibili `ic_SysBase` e
`ic_Services`. Lo scheletro rileva da solo il caso e stampa di conseguenza.

Per la domotica questo e' il primo punto da decidere: un driver display/GPIO che
tocca i registri richiede il profilo supervisor (oppure le syscall MMIO).

## Prossimi passi

- driver display HDMI (vedi `../SDK/docs/homeos/DISPLAY_AND_GPIO.md`);
- driver GPIO JH7110;
- input (tastiera) e shell;
- eventuale immagine di sistema passata come terzo argomento al bundler.
