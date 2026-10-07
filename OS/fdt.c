/*
 * HomeOS — lettore di device tree (FDT) minimale.
 *
 * Solo lettura, nessuna allocazione. Tutte le letture sono confinate al blob
 * dichiarato dalla sua intestazione: il DTB arriva dall'esterno, quindi va
 * trattato come dato non fidato.
 */

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

/* Token del blocco struttura (FDT_BEGIN_NODE, FDT_END_NODE, ...). */
#define FDT_T_BEGIN_NODE 1u
#define FDT_T_END_NODE   2u
#define FDT_T_PROP       3u
#define FDT_T_NOP        4u
#define FDT_T_END        9u

/* Offset dei campi dell'intestazione, tutti a 32 bit big-endian. */
#define FDT_HDR_TOTALSIZE       4u
#define FDT_HDR_OFF_STRUCT      8u
#define FDT_HDR_OFF_STRINGS     12u
#define FDT_HDR_VERSION         20u
#define FDT_HDR_BOOT_CPUID      28u
#define FDT_HDR_SIZE_STRINGS    32u
#define FDT_HDR_SIZE_STRUCT     36u
#define FDT_HDR_MIN_SIZE        40u

uint32_t fdt_u32(const void *pointer) {
    const uint8_t *bytes = (const uint8_t *)pointer;
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

uint64_t fdt_u64(const void *pointer) {
    const uint8_t *bytes = (const uint8_t *)pointer;
    return ((uint64_t)fdt_u32(bytes) << 32) | (uint64_t)fdt_u32(bytes + 4);
}

/* Una proprieta' e' una stringa se e' testo stampabile terminato da NUL.
 * Vale anche per le liste di stringhe ("compatible = a\0b\0"). */
static const char *fdt_as_string(const uint8_t *value, uint32_t len) {
    int printable = 0;

    if (!value || len < 2) return NULL;
    if (value[len - 1] != '\0') return NULL;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = value[i];
        if (c == '\0') continue;
        if (c < 0x20 || c > 0x7E) return NULL;
        printable = 1;
    }
    /* Serve almeno un carattere: cosi' una cella tutta a zero (#clock-cells
     * = <0>, pin-gpio-doen = <0>) resta un numero invece di sembrare una
     * stringa vuota. */
    return printable ? (const char *)value : NULL;
}

/* Il nome di una proprieta' vive nel blocco stringhe: deve essere terminato
 * dentro i propri limiti, altrimenti lo trattiamo come non disponibile. */
static const char *fdt_prop_name(const fdt_tree *tree, uint32_t offset) {
    const char *strings = (const char *)tree->base + tree->off_strings;

    if (offset >= tree->len_strings) return NULL;
    for (uint32_t i = offset; i < tree->len_strings; i++) {
        if (strings[i] == '\0') return strings + offset;
    }
    return NULL;
}

int fdt_open(fdt_tree *tree, uint64_t address) {
    const uint8_t *base;

    if (!tree) return -1;
    tree->base = NULL;
    if (address == 0) return -2;

    base = (const uint8_t *)(uintptr_t)address;
    if (fdt_u32(base) != FDT_MAGIC) return -3;

    tree->size = fdt_u32(base + FDT_HDR_TOTALSIZE);
    /* Non pretendiamo l'allineamento: i DTB reali in circolazione hanno
     * totalsize non multiplo di 4 (il JH7110 della VisionFive 2, per
     * esempio, misura 52701 byte). Contano soltanto i limiti. */
    if (tree->size < FDT_HDR_MIN_SIZE) return -4;

    tree->off_struct = fdt_u32(base + FDT_HDR_OFF_STRUCT);
    tree->len_struct = fdt_u32(base + FDT_HDR_SIZE_STRUCT);
    tree->off_strings = fdt_u32(base + FDT_HDR_OFF_STRINGS);
    tree->len_strings = fdt_u32(base + FDT_HDR_SIZE_STRINGS);
    tree->version = fdt_u32(base + FDT_HDR_VERSION);
    tree->boot_cpuid = fdt_u32(base + FDT_HDR_BOOT_CPUID);

    if (tree->off_struct + tree->len_struct > tree->size) return -5;
    if (tree->off_strings + tree->len_strings > tree->size) return -6;

    tree->base = base;
    return 0;
}

int fdt_walk(const fdt_tree *tree, fdt_visit_fn visit, void *user) {
    const uint8_t *structure;
    char path[FDT_PATH_MAX];
    uint32_t saved_len[FDT_MAX_DEPTH];
    uint32_t path_len = 0;
    uint32_t cursor = 0;
    int depth = 0;

    if (!tree || !tree->base || tree->len_struct == 0) return -1;

    structure = tree->base + tree->off_struct;
    path[0] = '\0';

    while (cursor + 4u <= tree->len_struct) {
        uint32_t token = fdt_u32(structure + cursor);
        cursor += 4u;

        if (token == FDT_T_BEGIN_NODE) {
            const char *name = (const char *)(structure + cursor);
            uint32_t name_len = 0;

            while (cursor + name_len < tree->len_struct && name[name_len] != '\0') {
                name_len++;
            }
            if (cursor + name_len >= tree->len_struct) return -2;  /* nome troncato */
            if (depth >= FDT_MAX_DEPTH) return -3;

            saved_len[depth] = path_len;
            if (path_len == 0) {
                /* Radice: il suo nome e' la stringa vuota. */
                path[0] = '/';
                path[1] = '\0';
                path_len = 1;
            } else {
                if (path_len + name_len + 2u > FDT_PATH_MAX) return -4;
                /* Sotto la radice non si aggiunge una seconda barra:
                 * il percorso della radice e' gia' "/". */
                if (!(path_len == 1 && path[0] == '/')) {
                    path[path_len++] = '/';
                }
                for (uint32_t i = 0; i < name_len; i++) {
                    path[path_len++] = name[i];
                }
                path[path_len] = '\0';
            }
            depth++;
            /* Il nome, NUL incluso, e' allineato a 4 byte. */
            cursor += (name_len + 4u) & ~3u;

        } else if (token == FDT_T_END_NODE) {
            if (depth > 0) {
                depth--;
                path_len = saved_len[depth];
                path[path_len] = '\0';
            }

        } else if (token == FDT_T_PROP) {
            uint32_t value_len, name_off, value_off;
            const char *prop_name;
            const uint8_t *value;

            if (cursor + 8u > tree->len_struct) return -5;
            value_len = fdt_u32(structure + cursor);
            name_off = fdt_u32(structure + cursor + 4u);
            value_off = cursor + 8u;
            if (value_off + value_len > tree->len_struct) return -6;

            value = structure + value_off;
            prop_name = fdt_prop_name(tree, name_off);
            if (!prop_name) return -9;  /* nome fuori dal blocco stringhe */

            if (visit &&
                visit(user, path, prop_name, value, value_len,
                      fdt_as_string(value, value_len)) != 0) {
                return 0;
            }
            cursor = value_off + ((value_len + 3u) & ~3u);

        } else if (token == FDT_T_NOP) {
            continue;

        } else if (token == FDT_T_END) {
            return 0;

        } else {
            return -7;  /* token sconosciuto: struttura non affidabile */
        }
    }

    return -8;  /* manca il token di fine */
}
