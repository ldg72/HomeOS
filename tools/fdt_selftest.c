/*
 * Collaudo del lettore FDT di HomeOS (OS/fdt.c) sul computer di sviluppo.
 *
 * Serve a verificare il cammino dell'albero su un device tree vero, prima di
 * provarlo sulla scheda: gli stessi file .dtb che U-Boot conosce per la
 * famiglia JH7110 stanno in EXEC64/platforms/MARS_BOOT/dtbs/.
 *
 * Compilazione ed esecuzione:
 *   cc -o /tmp/fdt_selftest tools/fdt_selftest.c OS/fdt.c -I OS
 *   /tmp/fdt_selftest file.dtb            # elenco dei nodi
 *   /tmp/fdt_selftest file.dtb usb        # nodi che contengono "usb"
 *   /tmp/fdt_selftest file.dtb sdio mmc   # piu' filtri, in OR
 *   /tmp/fdt_selftest file.dtb paths clock # solo i percorsi dei nodi
 *
 * Il codice sotto e' deliberatamente indipendente da HomeOS: usa solo il
 * lettore, quindi se questo test passa, l'errore non e' nel parser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fdt.h"

#define FILTER_MAX 8

static const char *g_filters[FILTER_MAX];
static int g_filter_count;
static int g_paths_only;
static char g_last_path[FDT_PATH_MAX];
static int g_nodes;
static int g_props;

static int matches_any(const char *path) {
    if (g_filter_count == 0) return 1;
    for (int i = 0; i < g_filter_count; i++) {
        if (strstr(path, g_filters[i])) return 1;
    }
    return 0;
}

static void print_value(const void *value, uint32_t len, const char *string) {
    const unsigned char *bytes = (const unsigned char *)value;

    if (len == 0) {
        printf("(empty)");
        return;
    }
    if (string) {
        uint32_t i = 0;
        int first = 1;
        while (i < len) {
            uint32_t start = i;
            while (i < len && bytes[i]) i++;
            if (!first) printf(", ");
            printf("\"");
            fwrite(bytes + start, 1, i - start, stdout);
            printf("\"");
            first = 0;
            i++;
        }
        return;
    }
    if (len <= 64 && (len % 4) == 0) {
        for (uint32_t i = 0; i < len; i += 4) {
            if (i) printf(" ");
            printf("0x%08x", fdt_u32(bytes + i));
        }
        return;
    }
    for (uint32_t i = 0; i < len && i < 64; i++) {
        if (i) printf(" ");
        printf("%02x", bytes[i]);
    }
    if (len > 64) printf(" ...");
}

static int visit(void *user, const char *path, const char *name,
                 const void *value, uint32_t len, const char *string) {
    (void)user;

    if (!matches_any(path)) return 0;

    if (strcmp(g_last_path, path) != 0) {
        snprintf(g_last_path, sizeof(g_last_path), "%s", path);
        if (g_paths_only) printf("%s\n", path);
        else printf("\n%s\n", path);
        g_nodes++;
    }
    if (g_paths_only) return 0;
    printf("  %s = ", name);
    print_value(value, len, string);
    printf("\n");
    g_props++;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s FILE.dtb [paths] [filtro...]\n", argv[0]);
        return 2;
    }

    int first = 2;
    if (argc > 2 && strcmp(argv[2], "paths") == 0) {
        g_paths_only = 1;
        first = 3;
    }
    for (int i = first; i < argc && g_filter_count < FILTER_MAX; i++) {
        g_filters[g_filter_count++] = argv[i];
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "non riesco ad aprire %s\n", argv[1]);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fprintf(stderr, "file vuoto\n");
        return 1;
    }

    unsigned char *buffer = malloc((size_t)size);
    if (!buffer || fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        fprintf(stderr, "lettura fallita\n");
        return 1;
    }
    fclose(file);

    fdt_tree tree;
    int rc = fdt_open(&tree, (uint64_t)(uintptr_t)buffer);
    if (rc != 0) {
        fprintf(stderr, "fdt_open: errore %d (magic %08x)\n", rc, fdt_u32(buffer));
        return 1;
    }
    printf("totalsize=%u  version=%u  struct=%u+%u  strings=%u+%u\n",
           tree.size, tree.version, tree.off_struct, tree.len_struct,
           tree.off_strings, tree.len_strings);

    if (tree.size > (uint32_t)size) {
        fprintf(stderr, "ATTENZIONE: totalsize (%u) supera il file (%ld)\n",
                tree.size, size);
        return 1;
    }

    g_last_path[0] = '\0';
    rc = fdt_walk(&tree, visit, NULL);
    if (g_paths_only) {
        printf("%d node(s)\n", g_nodes);
    } else {
        printf("\n-- %d nodi, %d proprieta', fdt_walk=%d\n", g_nodes, g_props, rc);
    }

    free(buffer);
    return rc == 0 ? 0 : 1;
}
