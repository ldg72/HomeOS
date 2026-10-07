# Display backend v1 — primo driver Mars

2026-10-04, branch `marsfb-jh7110`. Implementazione sperimentale verificata
sulla Milk-V Mars: il maintainer ha eseguito `displayprobe` e aperto il desktop
con `loadgfx 640 480`. Il bring-up precedente e MarsDemo 1.0 erano già stati
confermati fisicamente.

## Confine fra Core, device e librerie

Il Core Mars riserva il framebuffer fisico 0x70000000 e mappa esclusivamente
l'alias uncached 0x470000000 come supervisor RW/NX. Non inizializza più HDMI e
non contiene il renderer dimostrativo. I valori sono condivisi attraverso
`sdk/include/exec64/marsfb_memory.h`.

`os/devices/jh7110_display/` contiene la sequenza collaudata di alimentazione,
clock/reset, DC8200, PHY e HDMI, il device e la diagnostica. Il loader corrente
carica il device in S-mode. All'apertura il device verifica la traduzione Sv39
di ogni pagina del buffer; senza la riserva prevista rifiuta l'inizializzazione
prima di qualsiasi MMIO. Non esiste ancora un servizio generico di risorse
display/MMIO del Core: questa verifica è privata del backend Mars.

Graphics usa `DisplayRequest` da `sdk/include/devices/display.h`. I primi cinque
comandi e tutti gli offset restano compatibili con `VirtIOGPURequest`; l'header
legacy e l'interfaccia diretta VirtIO restano disponibili. `graphics.library`
non scopre più MMIO né chiama Init dell'interfaccia VirtIO. La selezione del
nome device viene fatta dalla build di piattaforma: `virtiogpu.device` su
QEMU, `jh7110display.device` su Mars. Non cambia la tabella pubblica IGraphics.

Non viene introdotta una syscall di rendering. Rimane il binding DoIO già
esistente: chiamata diretta in supervisor, syscall per il client user. Non
sono state cambiate politica di scheduling o configurazione dual-mode.

## Modi e buffer

QUERY_CAPS e QUERY_MODE restituiscono capacità del driver, non lo stato del
cavo o EDID. VirtIO mantiene dimensioni virtuali variabili e present asincrono;
Mars dichiara due modi fissi XRGB8888: 640×480 a circa 59,94 Hz, stride 2560,
e 1920×1080p60, stride 7680; entrambi sono verificati fisicamente. Il secondo
usa timing CEA e pre/post-PLL a 148,5 MHz dalla serie Linux JH7110 DRM v5.
Graphics controlla le capacità prima di creare lo schermo. Il default storico
di loadgfx non viene convertito automaticamente:
su Mars occorre specificare `loadgfx 640 480` oppure `loadgfx 1920 1080`.

Graphics disegna in RAM CPU cacheata. Mars copia i rettangoli modificati nel
buffer uncached riservato e applica il fence finale: il controller non legge
la copia cacheata. Il present iniziale è sincrono, senza pause fra blocchi e
senza scritture MMIO per rettangolo. Non offre IRQ/vblank, page flip, sincronismo
senza tearing o accelerazione. Non va confuso con il percorso VirtIO, che
mantiene il proprio worker, merge dei danni e backing corrente.

Un solo task possiede lo scanout e deve mantenere valido il backing fino a
RELEASE riuscito. Altri task non possono presentare, nascondere o rilasciare
quella superficie. STATUS diagnostico rimane leggibile; gli altri comandi
MarsFB vengono respinti mentre esiste un proprietario. Il gate è nonbloccante
perché DoIO può essere chiamato da syscall; Graphics ritenta brevemente un
PRESENT busy fuori dal device. Non c'è ancora recupero automatico della
proprietà se il proprietario termina in modo anomalo: in quel caso riavviare.

## Diagnostica e migrazione

`marsfb` diventa 3.0 e `marsdemo` 1.1: usano il device comune allo scanout. Il
report diagnostico resta ABI2. La syscall sperimentale SYS_MARSFB_DIAG è
riservata e restituisce unsupported, evitando un secondo proprietario MMIO nel
Core. **Aggiornare Core e bundle OS insieme**; i vecchi eseguibili diagnostici
non funzionano con il nuovo Core. La demo già confermata resta recuperabile
dal commit `6efc94d` (ricetta device RISC-V aggiunta in `c6f4fcf`).

La startup Mars continua a fermarsi alla shell. Dopo l'aggiornamento:

```text
displayprobe
marsdemo still
marsfb off
loadgfx 640 480
```

Per la prova del secondo modo, sostituire l'ultima riga con:

```text
loadgfx 1920 1080
```

`displayprobe` verifica capacità, indice modo e rifiuto delle query errate
senza inizializzare lo scanout. Dopo loadgfx si può usare `gfxconsole 400 240`
dalla seriale. Mouse/tastiera USB e cursore Mars non sono implementati da
questa modifica; input.device Mars rimane inerte. Non usare il comportamento
del puntatore come test del nuovo display.

## Verifiche

Esito 2026-10-04: build e verifiche automatiche sono PASS. Le build QEMU
sono isolate in `Build/DisplayChecks`; il bundle per la scheda è in
`Build/riscv64/milkv`. Contenuto SYS montato read-only e confrontato byte per
byte con device, graphics.library, comandi e startup appena compilati.


- `python3 tools/check_display_abi.py`: layout legacy/comune sulle due CPU.
- `python3 tools/check_marsfb.py`: sequenza hardware su bus simulato, demo/live,
  timeout e cleanup; inoltre i due modi, PLL/timing 1080p, proprietà, copia
  completa e parziale, coordinate, muting e release del vero handler DoIO Mars,
  con UBSan.
- Build complete per QEMU ARM64, QEMU RISC-V e Mars; prove QEMU con
  `tools/performance_boot_qemu.py --desktop` nei profili Protect e Performance.
- Test negativo QEMU RISC-V con il vero ELF Mars copiato su disco di test:
  il device rifiuta la riserva assente, diagnostici tornano unsupported, poi
  displayprobe VirtIO, loadgfx 640×480 e modecheck passano.
- Test fisico Milk-V Mars PASS: `displayprobe` riporta il modo fisso 640×480,
  stride 2560, 59940 millihertz e nessuna capacità non implementata; la query
  non attiva lo scanout. `loadgfx 640 480` mostra correttamente desktop,
  wallpaper, menu e volume SYS: sul monitor HDMI.
- Test fisico 1920×1080p60 PASS: `loadgfx 1920 1080` mantiene il segnale HDMI e
  mostra correttamente desktop, wallpaper, volume SYS: e GFX Console.

Passi successivi: recupero proprietario al task-exit, misure del costo della
copia, cursore/input, poi EDID e modi ulteriori. Un
worker/coalescing o un buffer mappato direttamente richiedono misure, non sono
automaticamente più veloci sulla memoria uncached.
