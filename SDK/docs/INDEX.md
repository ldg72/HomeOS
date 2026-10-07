# Indice documentazione SDK

## homeos/ — guide per il nuovo OS (leggi prima)

| File | Contenuto |
|---|---|
| `TOOLCHAIN.md` | toolchain su macOS e Linux (Windows escluso), installazione e verifica |
| `BOOT_CHAIN.md` | catena di boot esplicita: chi carica cosa e chi decide il nome dei file |
| `GETTING_STARTED.md` | come un OS si aggancia al Core: `_start`, `Exec64InitContext`, profili, syscall, memoria |
| `BOOT_IMAGE_FORMAT.md` | formato `BootBundle`/`BootInfo` e composizione di `HomeOS.img` |
| `DISPLAY_AND_GPIO.md` | framebuffer HDMI e GPIO: cosa esiste, cosa va implementato |

## core/ — contratti del Core (normativi)

| File | Contenuto |
|---|---|
| `EXEC64_KERNEL_FOUNDATION.md` | documento normativo di base |
| `exec64_core_boot_contract.md` | separazione Core/OS, artefatti di boot, Tranche |
| `exec64_init_abi.md` | entry point e `Exec64InitContext` v1.0/1.1/1.2 |
| `exec64_boot_resource_abi.md` | capability di boot a blocchi (`SYS_BOOT_RESOURCE_*`) |
| `exec64_bootstrap_init_supervision.md` | BootstrapSupervisor, Init, supervisione |
| `exec64_task_supervision_abi.md` | handle/eventi di terminazione dei task |
| `exec64_program_launch_abi.md` | contratto di avvio programmi |
| `exec64_core_os_repository_layout.md` | confini Core/OS e criteri di split |
| `iexec_library_api.md` | API IExec (liste, memoria, task, signal...) |
| `syscall_abi_reference.md` | numeri e semantica delle syscall (ecall) |
| `abi_library_contracts.md` | ordine/contratti delle vtable librerie |
| `core_performance_diagnostics.md` | contatori privati e protocollo baseline |

## hardware/ — Milk-V Mars (JH7110) e virtuale

| File | Contenuto |
|---|---|
| `hardware_setup_qemu_milkv.md` | setup QEMU e pinout seriale Mars |
| `elf_loading_and_build.md` | strategia ELF e comandi di build |
| `marsfb_v2.md` | specifica del framebuffer Mars |
| `jh7110_display_framebuffer_bringup.md` | sequenza di bring-up DC/VOUT/TMDS |
| `display_backend_v1.md` | ABI del backend display |
| `input_backend_dma_architecture.md` | input e contratto DMA |
| `STATUS_DISPLAY_MARS.md` | stato del bring-up display |
| `ROADMAP_DISPLAY_MARS.md` | roadmap display |
| `STATUS_USB_KEYBOARD_MARS.md` | stato tastiera USB |

## Nota sulla provenienza

Le guide in `homeos/` sono scritte appositamente per questa cartella. I documenti
in `core/` e `hardware/` sono copie del progetto originale e possono citare
**Exec64 OS** come consumatore di riferimento del Core: sono materiale di
contesto sul contratto, non istruzioni vincolanti per HomeOS. Dove un documento
descrive policy di Exec64 OS (DOS, shell, startup-sequence), quelle parti non si
applicano a un OS nuovo. Negli upstream il bundle può comparire con il nome
storico `Exec64OS.img`: per HomeOS è lo stesso oggetto chiamato `HomeOS.img`
(solo il nome cambia). Vedi `homeos/BOOT_CHAIN.md`.
