# Exec64 — Status Display Mars

Aggiornato: 2026-10-04. Branch sperimentale `marsfb-jh7110`, derivato da
`dual-mode`. Il lavoro dual-mode è temporaneamente sospeso per questo test.

## Stato corrente

Demo 1.0 e framebuffer 640×480 confermati dal maintainer sulla Mars. Anche il
primo `jh7110display.device` e il percorso graphics.library sono ora verificati
fisicamente sulla scheda.
La sequenza HDMI e il renderer demo sono stati spostati dal Core al device;
Core e bundle OS vanno aggiornati insieme. `marsfb` 3.0 e `marsdemo` 1.1 usano
lo stesso device e non possono interrompere uno scanout posseduto dalla grafica.

Graphics usa il contratto comune DisplayRequest, compatibile con il precedente
VirtIOGPURequest. La build sceglie VirtIO per QEMU e JH7110 per Mars. Il modo
640×480 e 1920×1080p60 sono verificati fisicamente; il secondo usa un pixel
clock di 148,5 MHz.
Entrambi usano XRGB8888 e copiano sincronicamente i rettangoli sporchi da RAM
cacheata al buffer uncached. Nessun EDID, vblank, cursore o input USB.
La startup rimane sulla shell. Test iniziale: `displayprobe`, `marsdemo still`,
`marsfb off`, poi `loadgfx 640 480`; per il nuovo test usare
`loadgfx 1920 1080`. Eventualmente `gfxconsole 400 240` da seriale.

Build complete dei tre target, test ABI e bus simulato, desktop QEMU Protect
e Performance su entrambe le CPU e verifica read-only del contenuto SYS PASS.
Il modulo Mars copiato intenzionalmente su QEMU viene rifiutato senza bloccare
VirtIO o la shell. Sulla Mars reale `displayprobe` espone correttamente l'unico
modo 640×480 XRGB8888 a 59,94 Hz e passa i controlli senza attivare lo scanout.
Il successivo `loadgfx 640 480` mostra via HDMI desktop, wallpaper, menu e SYS:.
Questo conferma il percorso completo graphics.library → device → DC8200 →
HDMI; non costituisce ancora una verifica di input USB o cursore.

## 1920×1080p60 verificato

Il secondo modo usa i timing CEA 1920×1080 a 60 Hz, sincronismi positivi e le
tuple pre/post-PLL a 148,5 MHz della serie Linux JH7110 DRM v5. La riserva
supervisor uncached è stata portata a 8.294.400 byte; inizia subito dopo gli
heap e resta entro il limite RAM minimo della piattaforma. Il test fake-bus
controlla PLL, registri HDMI, timing DC8200, stride 7680, dimensioni del piano,
copia completa e dirty rectangle. ABI display, test host e build Mars completa
passano. La prova fisica con `loadgfx 1920 1080` mostra correttamente desktop,
wallpaper, volume SYS: e GFX Console sul monitor HDMI.

[Driver, limiti, migrazione e test](../design/display_backend_v1.md).
[Bring-up precedente, mappa memoria e fonti](../arch/marsfb_v2.md).
Le sezioni seguenti conservano la cronologia delle prove già effettuate.

## Primo risultato sulla scheda e revisione 2.1

Il test 2.0 raggiunge i reset VOUT ma legge model=0 e si ferma; revisione e
customer non venivano acquisiti. Il logo firmware è visibile. La 2.1 legge
l'intero tripletto e permette model=0 soltanto con 5720/030e, in accordo con
l'identificazione per revisione del driver vendor e i precedenti dati Mars.
Nuovi casi host PASS; resta necessaria la nuova prova fisica con Core e OS 2.1.

## Secondo log e diagnostica 2.2

Identità 8200/5720/030e valida, PLL entrambi locked, HPD alto e fill completo.
La posizione scanout rimane zero; nessuna prova ancora di output visibile.
La 2.2 lascia l'uscita attiva con risultato INCONCLUSIVE e aggiunge `latch` per
confrontare isolatamente il bit0 di PANEL_CONFIG_EX. Report ampliato a ABI2:
servono entrambi i file Core e OS aggiornati. Test host estesi PASS.

## Primo framebuffer visibile e diagnostica 2.3

La 2.2 lasciata attiva produce blu stabile; anche rosso e verde sono corretti.
Il latch alternativo non è stato necessario. `marsfb live` aggiorna lo stesso
buffer uncached in 15 blocchi mentre scanout, PHY e HDMI restano attivi. Il test
host verifica che durante il cambio colore non avvengano scritture MMIO e che
panel e TMDS restino abilitati. La prova live fisica conferma cambi di colore
senza perdita del segnale. È visibile una progressione verticale, prevista dai
15 blocchi da 32 righe separati da DelayMs(1); non misura le prestazioni del
futuro backend.

## Comandi

- `marsfb status` / `marsfb dump`: snapshot; nessuna lettura dei blocchi non inizializzati.
- `marsfb poweron`: solo alimentazione VOUT.
- `marsfb test [blue|red|green|white|black]`: intera sequenza, default blu.
- `marsfb live [blue|red|green|white|black]`: cambia il buffer senza riavviare HDMI.
- `marsfb latch`: dopo test, porta solo il bit0 di DC 0x2518 a zero per confronto.
- `marsfb off`: ferma scanout/TMDS, conserva dominio/bus clock per diagnostica.
- `marsdemo [secondi|still]`: presentazione standalone; animazione predefinita
  di 30 secondi, massimo 120, poi lascia l'ultimo frame visibile.

## MarsDemo 1.0

`marsdemo` non usa graphics.library, Intuition o VirtIO e non anticipa l'ABI
del futuro display.device. Il comando U-mode orchestra la sequenza verificata;
il backend supervisor disegna nel framebuffer non cacheato. La schermata mostra
Exec64, Milk-V Mars, JH7110 HDMI, formato video e indicatori animati. I frame
animati non rileggono i registri diagnostici e non modificano clock, PLL o
controller. Test host, build e contenuto dell'immagine passano. Il maintainer
conferma la demo riuscita sulla scheda; la foto mostra correttamente testo,
gradiente, cornice e indicatori colorati sul monitor HDMI.

Aggiornare insieme Core e bundle OS sulla SD. Il vecchio Core non offre
SYS_MARSFB_DIAG; il nuovo comando segnala incompatibilità/unsupported.

## Perché il vecchio test non bastava

Il vecchio `test` faceva poweron e dump, senza programmazione PHY, timing o plane.
Le scritture SYSCRG includevano un registro di stato reset e valori globali;
la selezione HDMI nei bit bassi SYSCON non descriveva l'intero routing DPI.
Inoltre un normale buffer cacheato non avrebbe risolto la non-coerenza del DC.
La nota di marzo resta storica, mentre il riferimento operativo è MarsFB 2.0.

## Prossimo criterio di avanzamento

I due modi tramite graphics.library sono confermati. Il passo successivo è
integrare tastiera, mouse e cursore Mars. Misure e ottimizzazioni del present
precedono EDID e risoluzioni ulteriori.
