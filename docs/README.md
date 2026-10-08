# Documentazione / Documentation

Documenti **scritti da noi**, distinti dal materiale estratto da Exec64 che vive
in `SDK/`. Sono note di bring-up: cose verificate su hardware reale, con i
valori misurati e gli errori in cui si inciampa. Pensate per essere
riutilizzabili da chiunque scriva un OS sulla Milk-V Mars (StarFive JH7110).

Documents **written by us**, distinct from the material extracted from Exec64
under `SDK/`. They are bring-up notes: things verified on real hardware, with
the measured values and the traps you fall into. Meant to be reusable by anyone
writing an OS on the Milk-V Mars (StarFive JH7110).

## Indice / Index

| Documento / Document | Italiano | English |
|---|---|---|
| Tastiera USB — catena completa, valori di registro, le tre trappole | [ita/USB_KEYBOARD_MARS.md](ita/USB_KEYBOARD_MARS.md) | [eng/USB_KEYBOARD_MARS.md](eng/USB_KEYBOARD_MARS.md) |
| GPIO — mappa dei registri, connettore a 40 pin, self-test | [ita/GPIO_MARS.md](ita/GPIO_MARS.md) | [eng/GPIO_MARS.md](eng/GPIO_MARS.md) |
| Porte USB — due controller: Cadence sul SoC, USB 3.0 dietro PCIe | [ita/USB_PORTS_MARS.md](ita/USB_PORTS_MARS.md) | [eng/USB_PORTS_MARS.md](eng/USB_PORTS_MARS.md) |
| Cursore hardware — registri del DC8200, sequenza e due trappole | [ita/CURSOR_HARDWARE_MARS.md](ita/CURSOR_HARDWARE_MARS.md) | [eng/CURSOR_HARDWARE_MARS.md](eng/CURSOR_HARDWARE_MARS.md) |
| Memoria non cachata — il muro degli 8 MB a 1080p, e le strade per risolverlo | [ita/UNCACHED_MEMORY_MARS.md](ita/UNCACHED_MEMORY_MARS.md) | [eng/UNCACHED_MEMORY_MARS.md](eng/UNCACHED_MEMORY_MARS.md) |
| Limiti del Core emersi da un OS estraneo | [ita/CORE_NOTES_FROM_HOMEOS.md](ita/CORE_NOTES_FROM_HOMEOS.md) | [eng/CORE_NOTES_FROM_HOMEOS.md](eng/CORE_NOTES_FROM_HOMEOS.md) |
| Lacune del Core e richieste per chi sviluppa il kernel (storage) | [ita/CORE_REQUESTS.md](ita/CORE_REQUESTS.md) | [eng/CORE_REQUESTS.md](eng/CORE_REQUESTS.md) |

## Stato di HomeOS / HomeOS status

- gira su Milk-V Mars come payload `Init` in **supervisor (S-Mode)**;
- display HDMI XRGB8888 con driver proprio, 640×480 e 1920×1080p60;
- console di testo sul framebuffer, font Topaz (MIT);
- shell con un file per comando;
- **tastiera USB funzionante**: si scrive e si legge sull'HDMI senza seriale.

---

- runs on Milk-V Mars as the `Init` payload in **supervisor (S-Mode)**;
- HDMI display XRGB8888 with its own driver, 640×480 and 1920×1080p60;
- framebuffer text console, Topaz font (MIT);
- shell with one file per command;
- **USB keyboard working**: you type and read on HDMI without the serial line.

## Nota / Note

Le due versioni sono tenute allineate: se modifichi un documento, aggiorna
entrambe. — The two versions are kept in sync: if you change one, update both.
