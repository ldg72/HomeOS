# Exec64 - Init ABI v1

**Stato**: v1.2 supervisor validata su QEMU ARM64/HVF e RISC-V/TCG; v1.1 USER conservata.
La validazione Milk-V Mars citata sotto riguarda il checkpoint storico v1.1.
**Autorita**: [../ROADMAP/ROADMAP.md](../ROADMAP/ROADMAP.md)
**UAPI**: `core/include/uapi/exec64/init.h`

## 1. Scopo

L'Init ABI definisce il primo ingresso eseguito dall'OS dopo che Exec64 Core ha
validato e caricato il payload Init. Il contratto e indipendente da Exec64 OS:
un altro sistema puo fornire un proprio Init senza imporre al Core nomi file,
percorsi DOS o policy della shell.

`BootInfo` descrive il payload che il bootstrap consegna al Core. `InitContext`
descrive invece l'handoff runtime dal Core a Init nel dominio scelto dal profilo. I due contratti sono
versionati separatamente.

## 2. Entry point RISC-V e ARM64

Il payload espone `_start`. Al primo ingresso:

- `a0` su RISC-V, `x0` su ARM64, contiene un puntatore a `struct Exec64InitContext`
- gli altri argomenti non fanno parte della ABI v1.0
- il contesto resta valido per tutta la vita del task Init
- Init lo tratta come sola lettura
- un ritorno da `_start` equivale alla terminazione di Init ed e gestito dal
  `BootstrapSupervisor`

Il puntatore indica memoria appartenente allo stack del task Init. In Protect
lo stack e USER e il contesto non consegna puntatori supervisor.

## 3. Layout v1.0

`Exec64InitContext` occupa 64 byte:

| Offset | Campo | Tipo | Significato |
|---:|---|---|---|
| 0 | `ic_Magic` | `u64` | firma `EX64INIT` |
| 8 | `ic_AbiMajor` | `u16` | major ABI fornita dal Core |
| 10 | `ic_AbiMinor` | `u16` | minor ABI fornita dal Core |
| 12 | `ic_StructSize` | `u32` | byte validi della struttura |
| 16 | `ic_Flags` | `u64` | nessun flag definito in v1.0 |
| 24 | `ic_Reserved0..4` | `5 x u64` | devono essere zero |

La v1.0 certifica deliberatamente soltanto identita, versione, dimensione e
lifetime dell'handoff. Non espone indirizzi fisici, `ExecBase` del kernel,
strutture private, capability simulate o handle non ancora implementati.

## 4. Estensione v1.1

La v1.1 preserva il prefisso v1.0 da 64 byte e porta la struttura a 80 byte:

| Offset | Campo | Tipo | Significato |
|---:|---|---|---|
| 64 | `ic_BootResource` | `u64` opaco | capability della risorsa di boot |
| 72 | `ic_Reserved5` | `u64` | deve essere zero |

Il campo e valido soltanto quando `ic_Flags` contiene
`EXEC64_INIT_HAS_BOOT_RESOURCE`. In tal caso Init richiede minor almeno 1,
`ic_StructSize` almeno 80 e un handle diverso da zero.

La semantica dell'handle e definita in
[exec64_boot_resource_abi.md](exec64_boot_resource_abi.md). Il valore non e un
indirizzo fisico e non espone il backend QEMU/VirtIO o Milk-V Mars.

## 5. Compatibilita

- una major diversa e incompatibile
- Init rifiuta una minor maggiore di quella che conosce
- `ic_StructSize` deve coprire almeno il prefisso v1.0
- nuovi campi vengono aggiunti in coda e richiedono una nuova minor
- i campi riservati della v1.0 restano zero e non sostituiscono l'estensione
  append-only
- un Init v1.0 puo ignorare la coda v1.1 usando `ic_StructSize`; un Init v1.1
  rifiuta flag Boot Resource incoerenti con minor, size o handle
- il bootstrap continua a dichiarare nel descrittore BootInfo la versione Init
  richiesta dal payload; il Core la valida prima del caricamento

## 6. CRT Init e interfacce Core

Init usa il CRT dedicato `os/init/crt_init.c`. In Protect riceve il contesto
1.1, costruisce una copia locale USER di ExecBase e installa le interfacce
syscall. In Performance il contesto 1.2 consegna SysBase e i servizi diretti:
il CRT copia la base nel proprio payload, conservando gli indirizzi reali
delle interfacce e registrando il dominio supervisor. La copia permette a
Init di installare il proprio DOS senza mutare la base globale del Core.

L'entry C di Exec64 OS e quindi:

```c
void main(struct ExecBase *local_sysbase,
          const struct Exec64InitContext *context);
```

`local_sysbase` e una struttura del payload Init; `context` e il solo oggetto
fornito dal Core attraverso la Init ABI.

## 7. Estensione v1.2 supervisor (2026-10-02)

Il prefisso 1.1 resta di 80 byte. La struttura completa arriva a 96 byte:

| Offset | Campo | Tipo | Significato |
|---:|---|---|---|
| 80 | `ic_SysBase` | puntatore 64 bit | base Core, solo supervisor |
| 88 | `ic_Services` | puntatore 64 bit | servizi bootstrap diretti, solo supervisor |

Il flag `EXEC64_INIT_SUPERVISOR` richiede minor almeno 2, size almeno 96,
puntatori validi e una tabella `Exec64InitServices` versione 1 di 48 byte.
Questa contiene size/version e cinque funzioni: ResourceInfo, ResourceRead,
ResourceWrite, LoadImage, UnloadImage. Gli ultimi due operano su immagini
supervisor. I servizi vengono collegati una volta; le operazioni storage non
passano per syscall o per il servizio USER di Init. Rimangono i normali
controlli dei parametri e degli intervalli del backend.

Il Core consegna questo contratto soltanto dopo avere scelto insieme loader
ELF supervisor e task supervisor. In Protect consegna ancora minor 1/size 80,
con tutti i campi supervisor azzerati. Le API applicative restano identiche.
Dettagli del percorso e limiti nel [checkpoint Performance](performance_boot_cli.md).

## 8. Estensioni future

Le estensioni verranno introdotte solo insieme alla relativa implementazione:

- canale di supervisione per spawn/wait e notifica della terminazione shell
- descrizione delle capability realmente disponibili
- policy per componente nel profilo Mixed

Questi elementi non fanno parte della v1.1.

## 9. Gate

- [x] assert host su dimensione e offset UAPI
- [x] Init costruito con CRT dedicato
- [x] QEMU: contesto accettato, prompt raggiunto, `version` funzionante
- [x] QEMU: `exit` restituisce il controllo a Init e apre una nuova sessione
- [x] Milk-V Mars: bundle aggiornato fino al prompt con messaggio ABI v1
- [x] Milk-V Mars: `exit` seguito da una nuova sessione shell
- [x] QEMU v1.1: Boot Resource accettata e SYS montato da `exfat.handler`
- [x] QEMU v1.1: `dir` e caricamento ripetuto di un comando tramite
  `dos.library` OS-side
- [x] Milk-V Mars v1.1: Boot Resource memory-backed, mount e caricamento
  comando validati su hardware reale

- [x] QEMU v1.2, entrambe le CPU: Init/shell/CLI supervisor, RUN e nuova sessione dopo EXIT
- [x] QEMU: regressione Protect con contesto 1.1 e rifiuto promozione figlio USER
- [ ] v1.2 su Milk-V Mars fisico e SMP
