# Exec64 Core / Exec64 OS - Struttura del Repository

**Stato**: struttura e artefatti di boot separati; separazione funzionale in corso
**Modello iniziale**: monorepo con componenti costruibili separatamente

Il contratto proposto per separare anche gli artefatti di boot e descritto in
[exec64_core_boot_contract.md](exec64_core_boot_contract.md).

## Obiettivo

Separare in modo verificabile il kernel Exec64 Core dal sistema Exec64 OS senza
introdurre prematuramente due repository Git e il relativo costo di coordinamento.

La separazione deve permettere di:

- compilare e distribuire Exec64 Core come prodotto autonomo
- documentare e versionare la sua ABI pubblica
- compilare Exec64 OS contro una versione precisa della Core ABI
- comporre immagini QEMU e Milk-V Mars combinando artefatti gia costruiti
- consentire in futuro a sviluppatori indipendenti di usare Exec64 Core con un
  proprio `Init` o con un sistema alternativo

## Struttura Corrente

```text
EXEC64/
|-- core/
|   |-- kernel/
|   `-- arch/
|-- os/
|   |-- devices/
|   |-- libraries/
|   |-- shell/
|   |-- commands/
|   `-- assets/
|-- sdk/
|-- platforms/
|   `-- milkv-mars/
|-- tools/
|-- docs/
`-- Makefile
```

La prima fase ha spostato i sorgenti nei rispettivi alberi e ha riallineato il
build senza cambiare il comportamento runtime. I target pubblici completi
restano `make release-riscv`, `make release-milkv` e `make release-all`; i target
`core-qemu`, `os-qemu`, `os-mars`, `core-mars`, `image-qemu` e `image-mars`
rendono ora visibili le fasi di costruzione gia separabili.

Restano intenzionalmente da estrarre o riallineare:

- `core/include/private/` e `core/include/uapi/`
- `platforms/qemu/`, mentre bootstrap e linker QEMU restano in `core/arch/`
- normalizzazione finale degli output e del packaging QEMU

`core/kernel` contiene ancora servizi DOS, console e bootstrap transizionali.
La loro posizione corrente conserva la build ma non ne decide l'ownership
definitiva.

## Regole di Confine

- `core/` non deve acquisire nuove dipendenze da `os/`; quelle transizionali
  esistenti vanno rimosse in passi funzionali separati e verificabili.
- `os/` usa solo gli header pubblici della Core ABI e non include header privati
  del kernel.
- Core e OS devono poter essere compilati separatamente; il build corrente lo
  verifica per gli artefatti QEMU e Milk-V Mars, mentre il confine funzionale dei
  sorgenti resta ancora in evoluzione.
- Il sorgente del Core resta unico per QEMU e Mars; bootstrap, linker script e
  artefatti finali possono essere specifici della piattaforma.
- Una syscall o una struttura condivisa entra nell'ABI pubblica solo dopo essere
  stata documentata, versionata e verificata.
- La costruzione dell'immagine finale combina artefatti Core e OS senza
  ricompilazioni implicite che nascondano dipendenze tra i due componenti.

## Prodotti di Build Attesi

Il repository Core dovra poter produrre almeno:

```text
Exec64Core-qemu-riscv
Exec64Core-milkv-mars
exec64-core-sdk/
```

Il repository OS dovra produrre `Init`, driver, librerie, shell e comandi. Un
livello di composizione costruira poi le immagini avviabili, per esempio tramite:

```text
make core-qemu
make core-mars
make os-qemu
make os-mars
make image-qemu
make image-mars
```

I target QEMU Core e OS sono esposti separatamente. Anche `core-mars` costruisce
ora il Core senza `initrd.o`, mentre `os-mars` produce Init, l'immagine SYS
esterna e il bundle che li descrive. `image-mars` copia gli artefatti gia
costruiti e non rilinka il Core.

Il milestone `BootInfo v1` approvato mantiene inizialmente invariato il SYS exFAT
ma lo carica come artefatto esterno. Il nuovo target split dovra produrre:

```text
Build/Exec64.riscv
Build/Core/milkv-mars/Exec64Core
Build/OS/qemu/Exec64Boot.img
Build/OS/milkv-mars/Exec64Boot.img
Build/OS/milkv-mars/Exec64SYS.img
```

La composizione finale non rilinka il Core quando cambia soltanto l'immagine OS.
Il target `check-core-mars-hash` verifica automaticamente questa invariante. La
build Mars embedded resta disponibile tramite `release-milkv-legacy` finche il
percorso split non sara validato completamente sull'hardware reale.

## Bootstrap

Exec64 Core raggiunge il primo `Init` U-Mode prima di usare un nome o percorso
del filesystem dell'OS. Il contratto di bootstrap prevede:

- un `BootInfo` stabile tra bootloader, Core e primo processo
- un bundle di boot versionato con Init ELF64 obbligatorio e immagine SYS
  opzionale
- un percorso di errore leggibile quando `Init` manca o non e compatibile con la
  Core ABI

`BootInfo v1.1` consegna al Core il descrittore Init e l'eventuale descrittore
SYS. Il `BootstrapSupervisor` resta nel Core e avvia Init direttamente dalla
memoria; non cerca `System/Init` e non conosce la struttura interna dell'OS.
Init ABI v1.1 consegna poi a Init una Boot Resource opaca. Il primo
`dos.library` e il primo `exfat.handler` OS-side montano e leggono SYS in
U-Mode, usando lo stesso contratto con backend VirtIO/QEMU o RAM/Mars. Il
percorso legacy resta ancora nel Core come fallback e write path transitorio;
la sua rimozione richiede il gate reale su entrambe le piattaforme.

## Quando Separare Anche i Repository Git

Il passaggio a due repository distinti sara valutato solo quando:

- la Core ABI sara stabile e versionata
- Core e OS avranno cicli di rilascio realmente indipendenti
- Exec64 OS potra essere compilato usando soltanto un Core SDK pubblicato
- esistera un vantaggio concreto per sviluppatori o distributori esterni

Fino a quel momento il monorepo consente modifiche coordinate e commit atomici,
mantenendo comunque un confine architetturale controllabile dal build system.

## Stato ABI

La duplicazione della tabella syscall e stata risolta in `rev 0.114`:

- `sdk/include/exec/syscalls.h` e la sola tabella numerica autorevole
- `sdk/include/exec64_syscall.h` e un wrapper sorgente legacy
- `check-syscall-abi` protegge i numeri pubblici gia assegnati

Prima di distribuire Exec64 Core come contratto binario stabile restano da
definire la versione formale della Core ABI e la regola append-only.

## Criterio della Prima Fase

La riorganizzazione strutturale si considera valida soltanto se:

- QEMU raggiunge la shell U-Mode con il profilo operativo invariato
- `make release-milkv` produce e compone i due artefatti Mars separati
- nessuna funzione viene spostata tra Core e OS nello stesso cambiamento
- i percorsi precedenti restano recuperabili passando a un commit anteriore
