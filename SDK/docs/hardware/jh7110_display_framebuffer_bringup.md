# JH7110 Display Bring-up Notes

> Nota storica di marzo 2026. Per implementazione e test attuali usare
> [MarsFB 2.0](marsfb_v2.md), basato anche sulla serie Linux v5 di settembre.

**Stato**: nota tecnica di riferimento
**Data**: Marzo 2026
**Scopo**: raccogliere il percorso reale per attivare il display subsystem del JH7110 e capire come portare su un framebuffer nativo in Exec64.

---

## 1. Punto di partenza onesto

Con la documentazione pubblica del JH7110 si puo capire bene:
- dove vive il display subsystem
- quali clock/reset/syscon sono coinvolti
- quali blocchi fanno parte del dominio `VOUT`
- come viene instradata l'uscita verso HDMI / DSI / DPI

Con la documentazione pubblica **non** si ottiene invece una descrizione completa e autosufficiente dei registri del **DC8200** necessari per programmare in modo affidabile:
- plane/layer
- framebuffer base address
- stride
- pixel format
- scanout enable
- mode timing completi

Per questa parte il riferimento pratico deve essere il driver Linux StarFive / Verisilicon.

---

## 2. Fonti pubbliche da usare

### TRM pubblico JH7110

- TRM index:
  [JH7110 TRM](https://doc-en.rvspace.org/JH7110/TRM/)

- Display subsystem overview:
  [Display Subsystem](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/display_subsystem.html)

- Display memory map:
  [Memory Map](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/memory_map_display.html)

- Display clock/reset:
  [Clock and Reset](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/clock_n_reset_display.html)

- Display control registers landing page:
  [Control Registers](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/controller_registers_display.html)

- Display extra note / missing docs reference:
  [More Information](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/detail_info_display.html)

### StarFive display guides

- JH7110 SDK display guide:
  [DG Display](https://doc-en.rvspace.org/VisionFive2/DG_Display/)

- LCD/MIPI display driver locations:
  [Display Driver Locations](https://doc-en.rvspace.org/VisionFive2/DG_LCD/JH7110_SDK/display_driver_locations_lcd.html)

### Linux references

- Driver series introduction:
  [[PATCH 0/9] Add DRM driver for StarFive SoC JH7110](https://lists.infradead.org/pipermail/linux-riscv/2023-June/034419.html)

- `vs_fb.c` addition in the original series:
  [[PATCH 5/9] drm/verisilicon: Add mode config funcs](https://lists.infradead.org/pipermail/linux-riscv/2023-June/033104.html)

- Later DC8200 driver split / refresh:
  [LKML: Add Hardware Functions for VS DC8200](https://lkml.org/lkml/2024/11/20/426)

- Newer wrapper architecture around VOUT domain:
  [[PATCH RFC 00/13] drm: starfive: jh7110: Enable display subsystem](https://lists.infradead.org/pipermail/linux-riscv/2025-November/080592.html)

---

## 3. Cosa dice il TRM pubblico

### 3.1 Blocco display nel memory map

Dal TRM pubblico il display subsystem ruota intorno a questi blocchi:

- `DC8200 AHB0` a `0x2940_0000`
- `DC8200 AHB1` a `0x2948_0000`
- `DOM VOUT SYSCON` a `0x295B_0000`
- `DOM VOUT CRG` a `0x295C_0000`

Questa e la prima cosa utile per Exec64: sappiamo gia dove mappare il dominio display lato MMIO.

### 3.2 Domini e dipendenze

Il display subsystem non e un blocco isolato. Nella pratica servono almeno:

- power domain `VOUT`
- clock del blocco display/DC8200
- reset del dominio VOUT / display
- syscon per l'instradamento dell'uscita

### 3.3 DOM VOUT SYSCON

Il `DOM VOUT SYSCON` controlla la topologia di uscita:

- selezione panel 0 / panel 1
- instradamento `DP` vs `DPI`
- alcune scelte di bit depth / mapping colore
- comportamento dei path verso HDMI / LCD / MIPI

In altre parole:
- il `SYSCON` non programma il framebuffer
- ma decide **dove** il pixel stream del DC8200 va a finire

### 3.4 Limite del TRM pubblico

Il TRM pubblico rimanda esplicitamente a documenti aggiuntivi per il DC8200:

- `DC8200 Dual Display Controller DPU IP Exposed Accessible Registers`
- `DC8200 Dual Display Controller IP Hardware Features`

Questa e la conferma che il TRM pubblico da solo non basta per inizializzare in modo serio il framebuffer layer del controller.

---

## 4. Sequenza di bring-up realistica

Se l'obiettivo e vedere un framebuffer a schermo, la sequenza pratica e questa:

1. abilitare il **power domain VOUT**
2. deassert dei **reset** del display subsystem
3. abilitare i **clock** richiesti da DC8200 / HDMI / DSI / VOUTCRG
4. configurare `DOM VOUT SYSCON` per scegliere il path di uscita corretto
5. programmare il **DC8200**
   - framebuffer address
   - stride
   - pixel format
   - plane/layer enable
   - timing / mode
6. attivare il sink finale
   - HDMI
   - DSI
   - DPI/LCD

I passi 1-4 sono ricostruibili bene dal TRM e dal device tree Linux.
I passi 5-6 richiedono i driver Linux come riferimento primario.

---

## 4.1 Sequenza concreta gia verificata per accendere VOUT/DC8200

Questa e la parte oggi piu "certa", perche e supportata sia dal TRM pubblico
sia dall'analisi sperimentale sul JH7110 fatta via U-Boot.

### Power up del dominio VOUT

Base PMU: `0x1703_0000`

Sequenza osservata:

```text
mw 1703000c 0x10 1
mw 17030044 0xff 1
mw 17030044 0x05 1
mw 17030044 0x50 1
```

Interpretazione:

- `0x1703000c` seleziona il power domain da accendere
- `0x10` abilita `DOM_VOUT` (bit 4)
- `0x17030044` riceve la sequenza software-triggered `0xff -> 0x05 -> 0x50`

Registro di stato:

```text
md 17030080 1
```

Valore atteso dopo l'accensione:

```text
0x00000013
```

Lettura:

- bit 0 = system on
- bit 1 = CPU on
- bit 4 = VOUT on

Nota:
- la fonte sperimentale raccomanda una breve attesa dopo il power-up
- come riferimento prudente: ~50 ms

### Abilitazione clock VOUT (display subsystem)

Sequenza osservata:

```text
mw 13020028 0x80000000 1
mw 1302004c 0x80000000 1
mw 13020098 0x80000000 1
mw 1302009c 0x80000000 1
mw 130200e8 0x80000000 1
mw 130200f0 0x80000000 1
mw 130200f4 0x80000000 1
mw 130200f8 0x80000000 1
mw 130200fc 0x80000000 1
```

Interpretazione pratica:

- si tratta dell'abilitazione del root clock e dei clock del dominio VOUT
- la mappatura simbolica precisa va verificata con:
  - `Clock and Reset` del TRM
  - tree Linux / driver Linux

### Deassert reset VOUT (display subsystem)

Sequenza osservata:

```text
mw 130202fc 0x07e7f600 1
mw 13020308 0xfb9fffff 1
```

Questa sequenza porta il dominio VOUT in uno stato vivo a livello di clock/reset.

### Abilitazione clock DC8200 / path HDMI

Base `VOUT_CRG`: `0x295C_0000`

Sequenza osservata per il controller:

```text
mw 295C0010 0x80000000 1
mw 295C0014 0x80000000 1
mw 295C0018 0x80000000 1
mw 295C001c 0x80000000 1
mw 295C0020 0x80000000 1
mw 295C003c 0x80000000 1
mw 295C0040 0x80000000 1
mw 295C0044 0x80000000 1
mw 295C0048 0x00000000 1
```

Interpretazione pratica:

- i registri `0x10..0x44` abilitano i clock lato display controller / output path
- `0x48 = 0` deasserta i reset del blocco DC8200/HDMI usati nel test

### Verifica che il DC8200 sia vivo

Dump AHB0:

```text
md 29400000 0x20
```

Valori chiave da verificare:

- hardware revision a `AHB0 + 0x24`
- chip id a `AHB0 + 0x30`

Dump diretto:

```text
md 29400024 1   -> atteso 0x00005720
md 29400030 1   -> atteso 0x0000030e
```

Questa e la prova pratica minima che:

- power domain
- clock
- reset
- accesso MMIO del controller

sono tutti nello stato giusto.

---

## 5. Cosa guardare nei driver Linux

La documentazione StarFive indica che il codice di riferimento vive in:

`linux-5.15/linux/drivers/gpu/drm/verisilicon`

I file da leggere per primi sono:

- `vs_drv.c`
  - probe generale del driver DRM
  - orchestrazione del sottosistema

- `vs_dc.c`
  - livello controller/display core
  - bridge tra DRM e hardware programming

- `vs_dc_hw.c`
  - registri e programmazione effettiva del DC8200
  - questo e il file piu importante per framebuffer, plane e scanout

- `vs_plane.c`
  - configurazione plane/primary plane
  - formato pixel, address, stride, posizionamento

- `vs_fb.c`
  - lato framebuffer DRM / fbdev emulation / binding della memoria

- `vs_crtc.c`
  - mode set, timing, enable/disable della pipe

Se l'obiettivo Exec64 e il **primo framebuffer minimale**, il path utile e:

1. capire da `vs_drv.c` / `vs_dc.c` l'ordine di init
2. estrarre da `vs_dc_hw.c` e `vs_plane.c`:
   - base framebuffer
   - stride
   - pixel format
   - enable del primary plane
3. leggere da `vs_crtc.c` il punto in cui il controller viene davvero messo in running state

### 5.1 Sequenza Linux da ricostruire per un framebuffer minimale

Dal materiale pubblico e dall'analisi dei driver, il percorso minimo da seguire e:

1. `dc_bind`
   - power/clocks/reset base del dominio display

2. `dc_init`
   - bring-up iniziale del controller
   - lettura revision/chip id

3. `dc_hw_init`
   - init di plane e panel structs

4. `vs_dc_enable`
   - enable della pipeline attiva
   - setup display
   - dither / background / sync / output path

5. `dc_hw_setup_display`
   - timing, pannello, sync mode, background

6. `vs_dc_commit` / `dc_hw_commit`
   - commit del plane

7. `plane_ex_commit` / `plane_commit`
   - address framebuffer
   - stride
   - width / height
   - primary plane enable
   - scaling / offset / blend / ROI

Questa e la vera sequenza che Exec64 dovra riprodurre, ma in forma ridotta e non-DRM.

### 5.2 Cosa copiare per prima dal driver Linux

Per un primo driver framebuffer Exec64 non servono subito:

- overlay multipli
- cursor
- gamma LUT
- color management
- atomic DRM model

Serve invece copiare solo la parte minima che in Linux vive intorno a:

- `dc_hw_init`
- `dc_hw_setup_display`
- `dc_hw_commit`
- `plane_commit`

Obiettivo:

- un solo primary plane
- un solo framebuffer lineare
- un solo formato noto buono
- un solo mode fisso
- niente compositing avanzato

### 5.3 Ruolo reale dei file Linux da studiare

Per evitare di perdersi nel modello DRM, conviene leggere i file con questo filtro:

- `vs_drv.c`
  - entry point DRM/component master
  - utile per capire l'ordine di probe complessivo
  - **non** e il file principale per i registri framebuffer

- `vs_dc.c`
  - glue tra DRM e controller
  - contiene il path piu utile per Exec64:
    - `dc_bind`
    - `dc_init`
    - `vs_dc_enable`
    - `vs_dc_commit`
    - `vs_dc_update_plane`
  - e il posto giusto per capire la sequenza di alto livello

- `vs_dc_hw.c`
  - cuore vero della programmazione DC8200
  - contiene il valore maggiore per Exec64:
    - init hw
    - programmazione timing/display
    - registri del primary plane
    - address / stride / size / format
    - shadow/commit
  - e il file piu importante da tradurre

- `vs_plane.c`
  - traduce `drm_plane_state` in parametri hardware concreti
  - utile per capire:
    - come viene calcolato l'indirizzo del framebuffer
    - come viene passato il pitch/stride
    - quale pixel format viene scelto
    - quali campi sono davvero obbligatori per il primary plane

- `vs_crtc.c`
  - mostra il punto in cui il display viene davvero acceso/spento
  - utile soprattutto per:
    - `atomic_enable`
    - `atomic_disable`
    - commit/vblank path
  - importante per capire quando chiamare `vs_dc_enable` e `vs_dc_commit`

- `vs_fb.c`
  - serve soprattutto al lato DRM fbdev/framebuffer object
  - utile per capire il tipo di memoria attesa dal driver
  - **non** e il centro del bring-up minimo hardware

### 5.4 Cosa e gia chiaro dal driver Linux, anche senza copiare DRM

Dal materiale Linux disponibile emergono gia alcuni punti forti:

- il driver non e un semplice "framebuffer dumb" minimale
- usa il modello DRM/KMS con:
  - CRTC
  - plane
  - atomic state
  - commit/shadow register
- pero il percorso minimo hardware e isolabile abbastanza bene

Tradotto per Exec64:

- non serve portare DRM
- serve invece estrarre il sottoinsieme che Linux usa davvero per:
  - inizializzare il DC8200
  - configurare il display mode
  - programmare un primary plane lineare
  - fare il commit nei registri shadow/live

Il primo target sano per Exec64 resta quindi:

- un solo display attivo
- un solo primary plane
- un solo formato RGB noto buono
- un framebuffer lineare fisico
- un mode fisso noto buono
- nessun overlay/cursor/gamma/CSC avanzato se non strettamente necessario

---

## 6. Implicazioni per Exec64

### 6.1 Cosa possiamo fare subito

Gia oggi possiamo preparare in Exec64:

- mapping MMIO del display subsystem
- costanti base per:
  - `0x2940_0000`
  - `0x2948_0000`
  - `0x295B_0000`
  - `0x295C_0000`
- skeleton di init per:
  - power domain / reset / clock
  - `DOM VOUT SYSCON`

### 6.2 Cosa non conviene inventare

Non conviene indovinare a mano:

- registri plane del DC8200
- layout dei registri framebuffer
- bitfield enable/disable dei layer
- timing register set completo

Su questi punti il riferimento giusto deve essere:
- o documentazione DC8200 riservata
- o il driver Linux

### 6.3 Strategia sana per Exec64

La strategia migliore e:

1. usare il TRM per:
   - memory map
   - reset
   - clock
   - syscon / mux

2. usare Linux per:
   - DC8200 register programming
   - framebuffer plane bring-up
   - mode set reale

3. poi minimizzare
   - prima solo primary framebuffer
   - niente overlay
   - niente gamma/LUT
   - niente cursor plane

---

## 7. Primo target realistico

Il primo target Exec64 sensato non e:
- portare subito tutto il DRM model Linux

ma:
- power on del VOUT subsystem
- output path corretto
- un solo framebuffer lineare
- un solo mode fisso noto buono
- scanout stabile su HDMI oppure DSI

In pratica:

**bring-up minimale prima, astrazione grafica dopo**

---

## 7.2 Checklist minima Mars-only per vedere un colore pieno

Questa e la checklist pratica piu corta e utile oggi.
Va letta come sequenza candidata di bring-up minimo, non come patch kernel da applicare subito.

### Fase A — certo dal TRM pubblico + verifica empirica gia fatta

1. mappare i blocchi MMIO:
   - `DC8200 AHB0 = 0x2940_0000`
   - `DC8200 AHB1 = 0x2948_0000`
   - `DOM VOUT SYSCON = 0x295B_0000`
   - `DOM VOUT CRG = 0x295C_0000`
2. accendere `DOM_VOUT`
3. attendere stabilizzazione del dominio
4. abilitare i clock VOUT/display
5. deassertare i reset VOUT/display
6. abilitare clock/reset specifici del path DC8200/HDMI
7. verificare che il controller risponda:
   - revision attesa `0x5720`
   - chip id atteso `0x30e`

### Fase B — certa come struttura, ma da completare col driver Linux

8. configurare `DOM VOUT SYSCON` per instradare il pixel stream verso HDMI
9. inizializzare il DC8200 in stato noto:
   - background
   - timing/mode
   - output routing
   - shadow register policy
10. configurare un solo primary plane:
   - base address framebuffer
   - stride/pitch
   - width/height
   - pixel format RGB
   - position `(0,0)`
   - enable plane
11. fare commit dei registri del display controller
12. verificare a schermo un test semplicissimo:
   - full red
   - full green
   - full blue
   - pattern a bande

### Fase C — da tenere fuori dal primo test

- overlay planes
- cursor plane
- gamma LUT
- scaling
- color management avanzato
- console grafica
- integrazione con lo stack graphics/intuition

---

## 7.3 Cosa e certo, cosa e dedotto, cosa e ancora incerto

### Certo dal TRM pubblico / note gia verificate

- il display controller e il `DC8200`
- i blocchi MMIO principali sono:
  - `AHB0`
  - `AHB1`
  - `DOM VOUT SYSCON`
  - `DOM VOUT CRG`
- il bring-up richiede davvero:
  - power domain `VOUT`
  - clock
  - reset
  - syscon/mux
- la sola documentazione pubblica non basta per programmare in modo completo il plane/framebuffer
- la lettura di revision/chip-id del controller e una verifica minima sensata del fatto che il blocco sia vivo

### Dedotto con buona confidenza dal driver Linux

- l'ordine logico corretto e:
  - bind/init del controller
  - setup display mode/output
  - setup del primary plane
  - commit/shadow latch
- `vs_dc.c` contiene la sequenza di alto livello piu utile a Exec64
- `vs_dc_hw.c` contiene i registri realmente necessari per il primissimo framebuffer
- il driver Linux usa parecchie feature DRM, ma il sottoinsieme minimo per Exec64 e estraibile
- il formato iniziale piu prudente da privilegiare e un RGB 32-bit lineare tipo `XRGB8888` o equivalente supportato dal plane primario

### Ancora incerto o da verificare meglio

- quale combinazione esatta di bit in `DOM VOUT SYSCON` serva per il path HDMI specifico della Mars
- quale timing/mode fisso convenga usare come primo test reale
- quali registri del DC8200 siano strettamente obbligatori oltre a:
  - address
  - stride
  - size
  - format
  - enable
  - commit
- se il path HDMI della board richieda anche passaggi ulteriori lato bridge/PHY oltre al solo DC8200
- eventuali vincoli di allineamento del framebuffer stride/base address da rispettare per evitare scanout silenziosamente invalido

---

## 7.1 Primo deliverable realistico per Exec64

Per poter dire "abbiamo un framebuffer nativo funzionante sulla Mars", il primo deliverable dovrebbe essere:

1. accensione stabile di `DOM_VOUT`
2. clock/reset del DC8200 corretti
3. lettura corretta di:
   - revision `0x5720`
   - chip id `0x30e`
4. path di uscita fissato verso il sink scelto
5. primary plane configurato
6. framebuffer lineare in RAM visibile
7. test minimo:
   - riempimento colore pieno
   - pattern a bande
   - pixel write manuale noto

Fino a quel punto non serve ancora:
- GUI
- windowing
- librerie grafiche complete
- multi-plane
- accelerazione 2D/3D

---

## 8. Conclusione

Il JH7110 pubblico ci da abbastanza informazioni per:
- accendere il dominio display
- impostare clocks/reset
- configurare il mux di uscita

Ma non basta per programmare completamente il framebuffer del DC8200.

Per arrivare a un framebuffer nativo su Exec64 bisogna usare insieme:
- TRM pubblico JH7110
- device tree / driver Linux StarFive-Verisilicon

Questa nota va letta come base di orientamento:
- **TRM per l'infrastruttura**
- **Linux per il controller vero**

La parte oggi piu certa e gia riutilizzabile in Exec64 e:

- power-on `DOM_VOUT`
- enable clock/reset VOUT
- enable clock/reset DC8200
- verifica revision/chip id

La parte successiva da estrarre dai driver Linux e:

- programmazione del primary framebuffer plane
- commit del display mode
- scanout reale verso HDMI/DSI/DPI
