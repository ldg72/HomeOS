# MarsFB 2.3 — primo test HDMI JH7110

Nota storica: dal 2026-10-04 il percorso corrente è descritto in
[Display backend v1](../design/display_backend_v1.md). La sequenza seguente è
stata trasferita in jh7110display.device; il trasporto syscall qui descritto è
ritirato. MarsFB 3.0/MarsDemo 1.1 richiedono Core e OS aggiornati insieme.

Aggiornato: 2026-10-04. Branch sperimentale `marsfb-jh7110`, derivato da
`dual-mode`. Output 640×480 XRGB8888 fisicamente confermato sulla Mars. Non è
ancora un backend di graphics.library.

## 2.3 — aggiornamento del framebuffer durante lo scanout

La 2.2 è stata verificata sulla Mars con blu, rosso e verde corretti; il blu è
rimasto stabile oltre cinque minuti. SCAN_LOCATION è rimasto zero anche con
immagine valida, quindi quel registro non è un criterio affidabile di successo
su questo hardware. Il latch alternativo non è stato necessario.

`marsfb live [colore]` riscrive il framebuffer uncached in 15 blocchi senza
toccare i registri del DC8200, i PLL o HDMI e senza spegnere il pannello. Il
test host verifica l'assenza di scritture MMIO e la permanenza di panel/TMDS;
la prova fisica cambia correttamente tutti i colori senza perdita del segnale.
È visibile una progressione verticale: il diagnostico scrive deliberatamente
15 blocchi da 32 righe con DelayMs(1), su memoria non cacheata. Questo test
dimostra aggiornabilità e coerenza, non misura la velocità del driver finale.
Build completa Mars e contratti ABI PASS. Verificati in sola lettura il binario
2.3 dentro Exec64SYS.img, la startup senza LOADGFX e il Core pubblicato.
Build e smoke test QEMU RISC-V PASS: status, test e live sono rifiutati come
Mars-only; modecheck e KernelBench quick restano PASS.

## MarsDemo 1.0 — presentazione standalone

`marsdemo [secondi|still]` inizializza autonomamente il modo verificato e crea
una schermata 640×480 con gradiente, testo bitmap e indicatori animati. Il valore
predefinito è 30 secondi, il massimo 120; `still` lascia la composizione statica.
Al termine HDMI resta attivo. Il programma non usa lo stack grafico alto: è una
dimostrazione temporanea e separata dal futuro display.device.

Il renderer vive nel backend supervisor perché l'alias framebuffer resta
correttamente non accessibile al comando U-mode. Le operazioni sono ristrette a
costruzione della schermata e numero del frame. Il percorso caldo restituisce
solo il report ABI essenziale, evitando letture PMU/PLL/DC8200 a ogni frame.
La simulazione verifica che i frame non scrivano MMIO e che panel/TMDS restino
attivi. Build Mars e presenza del binario nell'immagine OS: PASS. Demo confermata
riuscita dal maintainer sulla Mars; foto di testo, gradiente e barre visibili
sul monitor HDMI ricevuta il 2026-10-04. Nessuna misura di frame rate implicita.

## 2.2 — scanout osservabile e confronto del latch pannello

Il secondo log hardware (2.1) legge 8200/5720/030e: il fallback model-zero non
è stato usato e la causa della prima lettura nulla resta aperta. Pre/post PLL
risultano locked, HPD bit7 è alto, fill completato, ma CURRENT_LOCATION resta
zero nei 40 ms campionati. Questo non prova ancora segnale video visibile né
misura l'effettiva frequenza pixel all'ingresso del DC.

La 2.1 spegneva subito l'uscita su quel timeout, rendendo difficile osservare
l'aggancio del monitor. La 2.2 distingue il caso **INCONCLUSIVE** dai timeout
PMU/reset/PLL: lascia il modo fisso richiesto attivo e torna alla shell. Non
stampa successo di scanout quando non osserva movimento. `off` resta disponibile.

Dopo `marsfb test`, attendere almeno 5 secondi e annotare lo stato del monitor,
poi eseguire `marsfb status`. Se il colore non è visibile, eseguire `marsfb latch`,
attendere di nuovo e salvare anche il secondo log. Il comando cambia soltanto
il bit0 di DC 0x2518 a zero, mantenendo PHY, clock e framebuffer. Il driver
StarFive U-Boot lo libera dopo la programmazione mentre il bridge generico
Linux lo scrive a uno come commit: si tratta di un confronto hardware A/B,
non di una causa già dimostrata. `test` ripristina la sequenza originale.

Il report ABI2 è di 144 byte: aggiunge PANEL_CONFIG_EX, FB_CONFIG (underflow
bit5), posizione iniziale/esito del campionamento e gate AXI/core/AHB/LCD.
Aggiornare **sia Core sia OS**. `status` conserva l'esito dell'ultima finestra
di campionamento ma legge nuovamente SCAN_LOCATION. Nessun IRQ viene abilitato.

Test host estesi: output mantenuto con contatore fermo, stop esplicito, latch
rifiutato prima dello start e mutazione di un solo registro nel test A/B.
Il bus simulato verifica il protocollo, non dimostra che il latch sia la causa
fisica sulla Mars. La conferma visiva rimane richiesta.

Verifica 2.2: test host PASS; build completa Mars con contratti ABI PASS.
Immagine SYS montata in sola lettura: binario MarsFB 2.2 identico al compilato,
startup identica alla sorgente senza LOADGFX. Core pubblicato identico al prodotto
della build. Le regressioni QEMU riportate sotto restano quelle della baseline 2.0.

Riscontro dell'utente sulla 2.1: logo VisionFive visibile al boot, poi monitor
senza segnale; durante `marsfb test` resta «cable not connect». HPD alto indica
il rilevamento del monitor lato scheda, non che il monitor riceva un segnale TMDS
valido. Nel codice corrente esaminato non risultano chiamate automatiche al
backend MarsFB al boot: la MMU prepara solo la mappatura RAM. Restano le vecchie
whitelist delle syscall diagnostiche MMIO, passive finché non invocate; il nuovo
comando usa la syscall dedicata. Il contenuto effettivo della SD e il firmware
installato non sono stati ispezionati da questo controllo.

## 2.1 — identificazione dopo il primo test fisico

Il primo log Mars si ferma al model 0x29400020: letto zero, atteso 0x8200.
La versione 2.0 interrompeva prima di leggere revision/customer e li stampava
come zero non acquisito: non dimostrava che l'intero blocco fosse inaccessibile.
Il logo firmware è visibile, ma non è una conferma dello scanout Exec64.

Riconfrontati i driver StarFive: `sf_vop.c:dc_hw_init` legge revision/customer;
`vs_dc_hw.c:dc_hw_init` seleziona DC_REV_0 dalla revisione 0x5720, senza controllare
il model. Il driver generico VeriSilicon usa invece anche il model 0x8200.
La 2.1 conserva e stampa il tripletto reale prima della decisione: consente
model 0 oppure 0x8200 solo con revisione 0x5720 e customer 0x030e, già osservati
sulla Mars in precedenza. Tutto zero, altre revisioni/customer o altro model
restano errori bloccanti. Il fallback è ristretto a questa diagnostica Mars;
non afferma che model zero sia universalmente normale sul JH7110.

Clock, reset, PLL, DMA e scanout restano quelli della 2.0 per isolare questa
verifica. Il test host copre ora model zero con coppia valida, tripletto zero,
revision/customer errati e conservazione degli identificativi nel report d'errore.
Build completa Mars 2.1 e contratti ABI inclusi: PASS. Verificati anche il
Core pubblicato e il binario 2.1 dentro l'immagine SYS montata in sola lettura.
Le regressioni QEMU restano quelle della baseline 2.0 riportate sotto.
La causa fisica resta aperta finché non arriva il nuovo log della scheda.

Fonti ufficiali:
- [StarFive U-Boot sf_vop.c](https://github.com/starfive-tech/u-boot/blob/JH7110_VisionFive2_devel/drivers/video/starfive/sf_vop.c)
- [StarFive Linux vs_dc_hw.c](https://github.com/starfive-tech/linux/blob/JH7110_VisionFive2_devel/drivers/gpu/drm/verisilicon/vs_dc_hw.c)

Il driver U-Boot contiene anche una routine `sf_vop_remove` che ferma il display,
asserisce reset e disabilita clock/alimentazione. È una spiegazione possibile
della scomparsa del logo al passaggio all'OS, non una diagnosi verificata del
firmware installato sulla Mars dell'utente.

## Uso sulla Mars

Compilare dalla radice del progetto:

```sh
make ARCH=riscv64 PLATFORM=milkv all
```

Copiare insieme sulla partizione di boot della SD:

- `Build/riscv64/milkv/Core/Exec64Core` → file indicato da `linux` in extlinux;
- `Build/riscv64/milkv/OS/milkv-mars/Exec64OS.img` → `/Exec64OS.img`,
  indicato da `initrd` in extlinux.

`Exec64OS.img` è il bundle Init + SYS; `Exec64SYS.img` da solo non lo sostituisce.
Se l'extlinux della SD usa ancora `linux /Exec64`, mantenere quel nome oppure
cambiare **quella riga** in `linux /Exec64Core`. Conservare DTB, indirizzi e
parametri del boot già funzionante. Non usare come sostituto dell'extlinux reale
i vecchi esempi Linux nella directory boot-config del repository.

La startup Mars ora si ferma alla shell: non esegue LOADGFX. Con monitor HDMI
collegato e acceso:

```text
marsfb status
marsfb test
marsfb test red
marsfb test green
marsfb live red
marsfb live green
marsfb off
```

Il colore predefinito è blu; sono ammessi anche white e black. `test` esegue
prima lo stop di un eventuale test precedente e riparte dai reset locali.
`poweron` accende soltanto PD_VOUT; non promette segnale video.
Gli alias brevi st/d/p/t restano; le vecchie scritture arbitrarie syscon/h0/h1
sono rimosse dal comando.

Conservare il log completo, indicando l'ultimo checkpoint e ciò che mostra il
monitor (assenza segnale, segnale con nero, colore corretto/errato). Il messaggio
di contatore in movimento compare solo quando osservato; INCONCLUSIVE non è
una conferma di scanout. Il successo del bring-up
richiede anche il colore visibile e stabile sulla scheda.

## Sequenza e differenze dal vecchio comando

1. PD_VOUT: bit 4 PMU, sequenza encourage ff/05/50, attesa stato con timeout.
2. Collegamento NoC e clock VOUT: divider AXI dedicato /5, gate selettivi,
   deassert SYS reset 26 e 43. I registri assert sono 0x2f8/0x2fc;
   0x308/0x30c sono **stato**, non destinazioni di scrittura.
3. VOUT APB /4, clock DC/HDMI, reset locali DC e HDMI ai bit 0/1/2/9 del
   registro 0x48; stato inverso a 0x4c. Verifica model DC8200=0x8200.
   Il reset locale ripulisce anche i plane/cursor precedenti. Nessun IRQ abilitato.
4. Routing panel 0 / DPI / RGB888: SYSCON4 mask 0x7e000000, value 0x0c000000;
   SYSCON8 bit 4 clear. Si preservano i bit estranei al routing video.
5. Release digital/analog HDMI e register-clock **TMDS**, necessario su JH7110.
   Registri HDMI byte-logici spaziati di 4 byte fisici. Nessuna scrittura ai
   registri dell'integrated PHY usati da altre varianti Innosilicon.
6. PHY: riferimento hardware 24 MHz, pre-PLL 25.175 MHz con frazione f55555;
   lock, pixel mux DC verso PHY, post-PLL per <=25.2 MHz, lock, LDO/serializer/TMDS.
7. Fill XRGB8888 uncached, 640×480, stride 2560, a blocchi di 32 righe; ritorno
   alla shell tra le chiamate e verifica degli estremi del buffer.
8. Timing VGA: H 640/656/752/800, V 480/490/492/525, H/V negativi.
   Primary plane 0 lineare, blend disabilitato, enable e shadow commit distinti,
   DPI RGB888, panel start e panel commit; video unmute, audio sempre mute.

È un segnale TMDS **DVI-compatible sul connettore HDMI**, senza EDID, audio,
InfoFrame o negoziazione del modo. HPD bit 7 è riportato, non è un requisito
bloccante del test forzato. Non equivale a un driver HDMI completo.

## Isolamento del test e memoria

La sequenza vive in `core/riscv64/arch/riscv64/boards/milkv/marsfb.c`, compilata
solo per BOARD_MILKV. Il comando U-mode usa SYS_MARSFB_DIAG (60) con operazione,
colore e report di 144 byte (ABI2); non passa indirizzi hardware o framebuffer.
Il dispatcher valida dimensione e buffer scrivibile. ARM64 e QEMU RISC-V
restituiscono unsupported senza accedere all'hardware. Nessuna modifica alla
policy di scheduling/dual-mode o ai percorsi grafici QEMU.

Il framebuffer fisico riservato è [0x70000000, 0x7012c000), subito oltre
l'heap [0x50000000, 0x70000000). Core e bundle di boot sono sotto 0x50000000.
Questa scelta presuppone l'attuale mappa Mars con almeno 1 GiB di RAM e
l'handoff U-Boot già funzionante; non introduce un allocatore DMA generale
né un parser di tutte le reserved-memory del DTB. Rivedere la riserva se si
cambia layout di boot/heap o si introduce un altro utilizzatore di quella RAM.

La CPU scrive solo all'alias fisico non cacheato 0x470000000, mappato RW/NX
supervisor prima della clonazione delle page table. La mappatura viene verificata
pagina per pagina; se non pronta, il fill viene rifiutato. Il DC usa l'indirizzo
basso 0x70000000 (DMA 32 bit), non l'alias alto. Nessun memset attraverso l'alias
cacheato. Barriere I/O ordinano RAM e registri; non vengono presentate come
flush della cache. Il DMA_SYNC generico resta invariato nel comportamento:
non è adatto a rendere coerente un buffer display cacheato su U74.

L'accesso è serializzato con try-lock, senza attesa del lock nella syscall.
I poll hanno scadenza: PMU 10 ms, reset 1 ms, ciascun PLL 100 ms, movimento
scanout 40 ms. Sono limiti di un test diagnostico, non latenze da mantenere
nel futuro device. Un timeout PMU/reset/PLL arresta il percorso e spegne gli output già
accessibili; il report conserva il registro e il valore della causa iniziale.
`off` spegne scanout e PHY, lasciando dominio e bus clock accesi per diagnostica;
non ripristina una eventuale immagine firmware precedente.

`status` non tenta letture DC/HDMI prima che il backend abbia verificato i clock
e i reset. I campi dei blocchi non ancora accessibili sono zero/non disponibili.
Un timeout software non può recuperare una transazione MMIO bloccata sul bus:
se la scheda si ferma prima del checkpoint, il log seriale precedente resta
essenziale. Non è stato installato un recovery da bus fault.

## Verifica riproducibile

`python3 tools/check_marsfb.py` compila la sequenza su host con bus simulato e
UndefinedBehaviorSanitizer: ordine/gating, preservazione dei reset estranei,
framebuffer intero, stop/retry, timeout PMU/reset/PLL/scanout, modello inatteso,
rifiuto del fill senza mappatura e argomenti non validi. Ogni processo ha timeout.
Non simula l'elettronica PHY né dimostra che i timing sono accettati dal monitor.
AddressSanitizer sul Mac di sviluppo si blocca nell'inizializzazione del runtime
prima di main; non viene quindi usato per questo test.

Verifiche eseguite il 2026-10-04 sulla versione 2.0 (baseline):

- build `all` e contratti ABI inclusi nelle build: Mars, QEMU RISC-V, QEMU ARM64;
- test host della sequenza: PASS;
- QEMU RISC-V/TCG: `marsfb status` e `marsfb test` restituiscono unsupported;
  seguono `modecheck` e `kernelbench quick`, entrambi PASS;
- QEMU ARM64/HVF: boot shell, `modecheck` e `kernelbench quick` PASS;
  build in `Build/MarsfbChecks` per lasciare aperta l'istanza ARM dell'utente;
- ispezione read-only di Exec64SYS.img Mars: binario marsfb identico a quello
  compilato, startup identica alla sorgente e nessun comando LOADGFX;
- Core Mars: 110640 byte; bundle Exec64OS.img: 54321152 byte.

Le prove QEMU usano un vCPU e dischi snapshot, profilo mixed-current. Non sono
una nuova validazione completa dei profili dual-mode né un test dell'hardware Mars.

## Riferimenti primari

- [Serie DRM JH7110 v5, 29 settembre 2026](https://mid.mail-archive.com/dri-devel%40lists.freedesktop.org/msg640286.html).
- [Controller HDMI, patch 16](https://www.mail-archive.com/dri-devel@lists.freedesktop.org/msg640283.html).
- [PHY, patch 19: tuple PLL e sequenza analogica](https://go.mail-archive.com/dri-devel%40lists.freedesktop.org/msg640319.html).
- [Device Tree, patch 20: topologia, pixel parent, dma-noncoherent](https://mid.mail-archive.com/dri-devel%40lists.freedesktop.org/msg640166.html).
- [StarFive TRM: VOUT CRG](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/dom_vout_crg.html)
  e [memory map](https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/system_memory_map.html).
- [Linux PMU](https://github.com/torvalds/linux/blob/master/drivers/pmdomain/starfive/jh71xx-pmu.c),
  [reset JH7110](https://github.com/torvalds/linux/blob/master/drivers/reset/starfive/reset-starfive-jh7110.c),
  [clock SYS](https://github.com/torvalds/linux/blob/master/drivers/clk/starfive/clk-starfive-jh7110-sys.c).
- [Alias non cacheato JH7110, discussione RISC-V](https://lists.infradead.org/pipermail/linux-riscv/2026-March/087202.html).

La v5 è una proposta Linux, non una garanzia di supporto upstream già rilasciato.
L'implementazione Exec64 resta deliberatamente limitata al modo fisso; il
backend estratto in `jh7110display.device` e il desktop di graphics.library
sono stati successivamente verificati sulla Mars il 2026-10-04.
