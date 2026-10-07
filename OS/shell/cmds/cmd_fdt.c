/*
 * Comando 'fdt' — legge il device tree che la scheda consegna all'avvio.
 *
 * E' lo strumento con cui guardiamo l'hardware invece di indovinarlo: quali
 * controller USB esistono e a che indirizzo, qual e' il nodo della microSD,
 * quali clock e reset servono a ognuno.
 */

#include <exec/exec_base.h>

#include "../command.h"
#include "../shell.h"
#include "../../fdt.h"
#include "../../services.h"

/* ---------------------------------------------------------------- utilita' */

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int str_starts(const char *text, const char *prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static int str_contains(const char *text, const char *needle) {
    if (!*needle) return 1;
    for (; *text; text++) {
        const char *t = text;
        const char *n = needle;
        while (*t && *n && *t == *n) {
            t++;
            n++;
        }
        if (!*n) return 1;
    }
    return 0;
}

static void put_hex_byte(uint8_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char text[3];
    text[0] = digits[(value >> 4) & 0xF];
    text[1] = digits[value & 0xF];
    text[2] = '\0';
    shell_puts(text);
}

static void copy_path(char *destination, const char *source) {
    uint32_t i = 0;
    while (source[i] && i + 1u < FDT_PATH_MAX) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

static void put_hex_cell(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char text[11];
    text[0] = '0';
    text[1] = 'x';
    for (int i = 0; i < 8; i++) {
        text[2 + i] = digits[(value >> ((7 - i) * 4)) & 0xF];
    }
    text[10] = '\0';
    shell_puts(text);
}

/*
 * Stampa un valore di proprieta' nel modo piu' leggibile possibile:
 *   - stringa (anche lista): fra virgolette, separate da virgola;
 *   - fino a 8 celle: come parole da 32 bit, che e' come si legge 'reg';
 *   - altrimenti: byte esadecimali, con taglio oltre i 64.
 */
static void put_value(const void *value, uint32_t length, const char *string) {
    const uint8_t *bytes = (const uint8_t *)value;

    if (length == 0) {
        shell_puts("(empty)");
        return;
    }

    if (string) {
        uint32_t i = 0;
        int first = 1;
        while (i < length) {
            uint32_t start = i;
            while (i < length && bytes[i] != '\0') i++;
            if (!first) shell_puts(", ");
            shell_putc('"');
            for (uint32_t k = start; k < i; k++) shell_putc((char)bytes[k]);
            shell_putc('"');
            first = 0;
            i++;
        }
        return;
    }

    if (length != 0 && length <= 64u && (length & 3u) == 0u) {
        for (uint32_t i = 0; i < length; i += 4u) {
            if (i) shell_putc(' ');
            put_hex_cell(fdt_u32(bytes + i));
        }
        return;
    }

    for (uint32_t i = 0; i < length && i < 64u; i++) {
        if (i) shell_putc(' ');
        put_hex_byte(bytes[i]);
    }
    if (length > 64u) shell_puts(" ...");
}

/* ------------------------------------------------- riepilogo ('fdt' nudo) */

static struct {
    char last_path[FDT_PATH_MAX];
} g_summary;

static int summary_visit(void *user, const char *path, const char *name,
                         const void *value, uint32_t length, const char *string) {
    (void)user;

    if (str_eq(path, "/") && (str_eq(name, "model") || str_eq(name, "compatible"))) {
        shell_puts("  ");
        shell_puts(name);
        shell_puts(" : ");
        put_value(value, length, string);
        shell_puts("\n");
        return 0;
    }

    if (str_starts(path, "/memory")) {
        if (!str_eq(g_summary.last_path, path)) {
            copy_path(g_summary.last_path, path);
            shell_puts("  node ");
            shell_puts(path);
            shell_puts("\n");
        }
        shell_puts("    ");
        shell_puts(name);
        shell_puts(" = ");
        put_value(value, length, string);
        shell_puts("\n");
    }
    return 0;
}

/* ------------------------------------------------------ ricerca ('fdt ...') */

/*
 * Il filtro e' testuale sul percorso del nodo, quindi e' facile che peschi
 * troppo: 'fdt mmc' trova i venti nodi di pinmux che contengono "mmc", non i
 * controller (che si chiamano sdio0/sdio1). Due rimedi, entrambi qui:
 * piu' filtri in OR ("fdt sdio mmc") e la modalita' 'paths', che stampa solo
 * i percorsi dei nodi: prima si guarda la mappa, poi si chiede un nodo solo.
 */
#define FDT_FILTER_MAX 6

static struct {
    const char *filters[FDT_FILTER_MAX];
    int filter_count;
    int paths_only;
    char last_path[FDT_PATH_MAX];
    int matched;
    int properties;
} g_find;

static int matches_any(const char *path) {
    for (int i = 0; i < g_find.filter_count; i++) {
        if (str_contains(path, g_find.filters[i])) return 1;
    }
    return 0;
}

static int find_visit(void *user, const char *path, const char *name,
                      const void *value, uint32_t length, const char *string) {
    (void)user;

    if (!matches_any(path)) return 0;

    if (!str_eq(g_find.last_path, path)) {
        copy_path(g_find.last_path, path);
        if (g_find.paths_only) {
            shell_puts(path);
            shell_puts("\n");
        } else {
            shell_puts("\n");
            shell_puts(path);
            shell_puts("\n");
        }
        g_find.matched++;
        g_find.properties = 0;
    }

    if (g_find.paths_only) return 0;

    shell_puts("  ");
    shell_puts(name);
    shell_puts(" = ");
    put_value(value, length, string);
    shell_puts("\n");
    g_find.properties++;
    return 0;
}

/* ------------------------------------------------------------------ comando */

static void fdt_usage(void) {
    shell_puts("usage: fdt                      summary\n");
    shell_puts("       fdt <text> [<text>...]   nodes and their properties\n");
    shell_puts("       fdt paths <text> [...]   node paths only\n");
    shell_puts("examples: fdt usb    fdt sdio mmc    fdt paths clock\n");
}

SHELL_COMMAND(cmd_fdt, "fdt", "device tree: 'fdt' summary, 'fdt <text>' search") {
    struct ExecBase *sysbase = services_sysbase();
    uint64_t address = sysbase ? (uint64_t)sysbase->ex_PlatformData : 0;
    fdt_tree tree;
    int rc;

    if (address == 0) {
        shell_puts("no platform data pointer in SysBase (U-Mode?)\n");
        return;
    }

    rc = fdt_open(&tree, address);
    if (rc != 0) {
        shell_puts("device tree not usable at ");
        shell_puthex64(address);
        shell_puts(" (error ");
        shell_putu32((uint32_t)(-rc));
        shell_puts(")\n");
        return;
    }

    if (argc == 1) {
        shell_puts("device tree at ");
        shell_puthex64(address);
        shell_puts("\n");
        shell_puts("  totalsize : ");
        shell_putu32(tree.size);
        shell_puts(" byte   version : ");
        shell_putu32(tree.version);
        shell_puts("   boot cpu : ");
        shell_putu32(tree.boot_cpuid);
        shell_puts("\n");

        g_summary.last_path[0] = '\0';
        rc = fdt_walk(&tree, summary_visit, NULL);
        if (rc != 0) {
            shell_puts("walk stopped early (error ");
            shell_putu32((uint32_t)(-rc));
            shell_puts(")\n");
        }
        shell_puts("try: fdt usb   fdt sdio   fdt paths clock   fdt find chosen\n");
        return;
    }

    int first = 1;
    int paths_only = 0;
    if (str_eq(argv[1], "find")) {
        first = 2;
    } else if (str_eq(argv[1], "paths")) {
        first = 2;
        paths_only = 1;
    }
    if (first >= argc) {
        fdt_usage();
        return;
    }
    if (argc - first > FDT_FILTER_MAX) {
        shell_puts("too many filters (max ");
        shell_putu32((uint32_t)FDT_FILTER_MAX);
        shell_puts(")\n");
        return;
    }

    g_find.last_path[0] = '\0';
    g_find.matched = 0;
    g_find.properties = 0;
    g_find.paths_only = paths_only;
    g_find.filter_count = 0;
    for (int i = first; i < argc; i++) {
        g_find.filters[g_find.filter_count++] = argv[i];
    }

    rc = fdt_walk(&tree, find_visit, NULL);

    if (g_find.matched == 0) {
        shell_puts("no node matches");
        for (int i = 0; i < g_find.filter_count; i++) {
            shell_puts(i ? " " : " \"");
            shell_puts(g_find.filters[i]);
            if (i + 1 == g_find.filter_count) shell_putc('"');
        }
        shell_puts("\n");
    } else {
        shell_puts("\n");
        shell_putu32((uint32_t)g_find.matched);
        shell_puts(" node(s)");
        if (!g_find.paths_only) {
            shell_puts(", ");
            shell_putu32((uint32_t)g_find.properties);
            shell_puts(" properties");
        }
        shell_puts("\n");
    }
    if (rc != 0) {
        shell_puts("walk stopped early (error ");
        shell_putu32((uint32_t)(-rc));
        shell_puts(")\n");
    }
}
