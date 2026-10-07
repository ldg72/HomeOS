#ifndef HOMEOS_FDT_H
#define HOMEOS_FDT_H

#include <stdint.h>

/*
 * Lettore minimale di device tree (Flattened Device Tree) in sola lettura.
 *
 * La scheda ci consegna il proprio device tree all'avvio, e il Core ce ne
 * passa l'indirizzo fisico in SysBase->ex_PlatformData. E' la descrizione
 * dell'hardware piu' autorevole che possiamo avere: l'ha scritta chi conosce
 * la scheda, non un documento di terzi.
 *
 * Proprieta' di questo lettore:
 *   - non alloca memoria e non modifica nulla;
 *   - ogni accesso e' limitato al blob dichiarato (totalsize): un DTB
 *     troncato o corrotto produce un errore, non una lettura fuori range;
 *   - i campi del DTB sono big-endian, quindi si passa sempre da fdt_u32/u64.
 */

#define FDT_MAGIC          0xd00dfeedUL
#define FDT_PATH_MAX       256
#define FDT_MAX_DEPTH      32

uint32_t fdt_u32(const void *pointer);
uint64_t fdt_u64(const void *pointer);

typedef struct {
    const uint8_t *base;
    uint32_t size;         /* totalsize */
    uint32_t version;
    uint32_t boot_cpuid;
    uint32_t off_struct;
    uint32_t len_struct;
    uint32_t off_strings;
    uint32_t len_strings;
} fdt_tree;

/*
 * Apre il DTB all'indirizzo fisico indicato.
 * Ritorna 0 se il blob e' valido e interamente contenuto nei propri limiti.
 */
int fdt_open(fdt_tree *tree, uint64_t address);

/*
 * Visitatore, chiamato una volta per ogni proprieta'.
 *
 *   node_path  percorso del nodo, es. "/soc/usb@10110000"
 *   prop_name  nome della proprieta', es. "reg"
 *   value      valore grezzo (big-endian, come nel DTB)
 *   value_len  lunghezza in byte
 *   string     valore interpretato come stringa, oppure NULL se non lo e'
 *
 * Ritorna 0 per continuare, 1 per interrompere la visita.
 */
typedef int (*fdt_visit_fn)(void *user, const char *node_path,
                            const char *prop_name,
                            const void *value, uint32_t value_len,
                            const char *string);

/*
 * Cammina l'intero albero chiamando il visitatore.
 * Ritorna 0 a fine albero, negativo se il blocco struttura non e' coerente.
 */
int fdt_walk(const fdt_tree *tree, fdt_visit_fn visit, void *user);

#endif
