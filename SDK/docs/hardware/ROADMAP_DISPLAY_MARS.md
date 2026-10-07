# Exec64 — Roadmap Display Mars

**Aggiornato**: 2026-10-04
**Ambito**: percorso minimo e pratico per arrivare a un primo framebuffer visibile sulla Milk-V Mars.

**Regola di governance**
- questa roadmap non puo scavalcare [ROADMAP.md](ROADMAP.md), che resta la priorita master del core
- se un passo di questo filone implica modifiche al core kernel, il collaboratore AI deve fermarsi e aprire una discussione con il maintainer prima di procedere
- se la modifica al core viene approvata, il fatto va registrato anche in [../STATUS/STATUS.md](../STATUS/STATUS.md) indicando che scaturisce dalla lavorazione di `ROADMAP_DISPLAY_MARS.md`

---

## Aggiornamento operativo — MarsFB 2.3

Autorizzata la minima estensione Core necessaria al comando U-mode: dispatcher
con operazioni ristrette, backend nella directory board e riserva framebuffer
supervisor uncached. Registrato anche nello STATUS master. Le fasi C/D sono
implementate e validate fisicamente a 640×480: blu, rosso e verde corretti,
con blu stabile oltre cinque minuti. La fase E è iniziata con la prova di
aggiornamento live, superata senza perdita del segnale; può iniziare
l'estrazione del device.
[Procedura corrente](../arch/marsfb_v2.md).

Come dimostrazione intermedia è disponibile `marsdemo`: usa esclusivamente il
backend diagnostico verificato e non modifica il progetto del futuro device.

## 1. Obiettivo del blocco

Arrivare a un primo risultato verificabile:

- uscita video della Mars attiva
- framebuffer lineare minimale
- colore pieno o pattern fisso visibile

Questo blocco non include ancora:

- stack grafico alto
- screen/window system
- `graphics.library`
- driver definitivo generalizzato

---

## 2. Strategia scelta

La strategia scelta e:

1. comando diagnostico `marsfb`
2. sequenza hardware minima Mars-only
3. test manuali ripetibili
4. solo dopo eventuale astrazione in backend o device

---

## 3. Ordine di lavoro

### Fase A — Strumento minimo

Creare `marsfb` come comando diagnostico standalone.

Obiettivi:

- entrypoint chiaro
- help/usage
- perimetro separato dal kernel core
- nessun accoppiamento con lo stack grafico alto

### Fase B — Diagnostica passiva

Portare `marsfb` a stampare:

- basi MMIO rilevanti
- registri chiave di power/VOUT
- stato clock/reset conosciuto

Obiettivo:

- avere un punto di partenza osservabile e ripetibile

Nota pratica emersa dai test:

- sulla Mars, un comando U-Mode non puo dereferenziare direttamente i registri fisici del blocco display
- la diagnostica passiva deve quindi usare un ponte supervisor minimo, dedicato al solo probe MMIO
- la stessa logica vale per le write: usare solo una whitelist stretta di registri e sequenze note

### Fase C — Bring-up VOUT base

Implementare nel comando la parte piu certa:

- power domain VOUT
- clock display/VOUT
- reset display/VOUT
- lettura e stampa dei registri dopo ogni passo

Obiettivo:

- sapere con confidenza che il dominio display e acceso

Vincolo:

- anche le scritture dovranno passare da un helper supervisor ristretto e whitelistato

### Fase D — Primo framebuffer minimale

Quando i riferimenti DC8200 sono abbastanza chiari:

- scegliere un framebuffer lineare
- impostare base/stride/formato
- provare un primary plane minimale
- usare fill rosso/verde/blu o stripes

Obiettivo:

- primo segnale visibile a schermo

### Fase E — Consolidamento

Solo dopo il primo output visibile:

- pulire la sequenza
- separare costanti/registri
- valutare se estrarre un `marsfb.device`
- decidere se e quando esporre un backend verso il resto del sistema

---

## 4. Criterio di successo

Il blocco puo dirsi riuscito quando passa questo test:

1. boot shell sulla Mars
2. esecuzione manuale di `marsfb test`
3. comparsa a video di un colore pieno o pattern noto
4. output seriale coerente con la sequenza eseguita

Finche questo non succede, non ha senso parlare di integrazione nello stack grafico alto.

---

## 5. Cosa evitare

- non usare `loadgfx` come primo test
- non toccare `graphics.library` per il bring-up iniziale
- non trattare il framebuffer Mars come problema del kernel core
- non spostare subito il lavoro in un driver finale
- non mescolare il debug display con altri filoni come MicroPython o net.device

---

## 6. Domande da chiudere nel tempo

- quale uscita finale e la piu realistica per il primo successo sulla board usata
- quali registri minimi del `DC8200` bastano per un primary plane lineare
- quali vincoli reali esistono su:
  - stride
  - allineamento base framebuffer
  - pixel format
  - timing/mode

---

## 7. Punto pratico di ripartenza

Se si riapre questo filone in un nuovo contesto:

- leggere prima [../STATUS/STATUS_DISPLAY_MARS.md](../STATUS/STATUS_DISPLAY_MARS.md)
- poi leggere [../arch/jh7110_display_framebuffer_bringup.md](../arch/jh7110_display_framebuffer_bringup.md)
- poi lavorare solo su `marsfb`, non sulle librerie grafiche alte
