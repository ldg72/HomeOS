/*
 * HomeOS — shell.
 *
 * Loop: prompt, lettura di una riga (SYS_KGETC, bloccante), divisione in
 * argomenti, dispatch sul comando registrato nella sezione .commands.
 */

#include <stddef.h>

#include "shell.h"
#include "../console.h"
#include "../cursor.h"
#include "../fbcon.h"
#include "../os_syscalls.h"
#include "../theme.h"
#include "../usb/hid.h"
#include "../usb/usb.h"

#define LINE_MAX 128
#define ARG_MAX  8

/*
 * Tabella dei comandi. Un comando nuovo = un file in cmds/ piu' una riga qui.
 */
extern const struct shell_command cmd_help;
extern const struct shell_command cmd_info;
extern const struct shell_command cmd_echo;
extern const struct shell_command cmd_logo;
extern const struct shell_command cmd_bars;
extern const struct shell_command cmd_mem;
extern const struct shell_command cmd_clear;
extern const struct shell_command cmd_usb;
extern const struct shell_command cmd_mode;
extern const struct shell_command cmd_gpio;
extern const struct shell_command cmd_theme;
extern const struct shell_command cmd_retro;
extern const struct shell_command cmd_modern;
extern const struct shell_command cmd_reboot;
extern const struct shell_command cmd_fdt;
extern const struct shell_command cmd_cursor;

static const struct shell_command *const g_commands[] = {
    &cmd_help,
    &cmd_info,
    &cmd_echo,
    &cmd_logo,
    &cmd_bars,
    &cmd_mem,
    &cmd_clear,
    &cmd_usb,
    &cmd_mode,
    &cmd_gpio,
    &cmd_theme,
    &cmd_retro,
    &cmd_modern,
    &cmd_reboot,
    &cmd_fdt,
    &cmd_cursor,
};

#define COMMAND_COUNT ((int)(sizeof(g_commands) / sizeof(g_commands[0])))

int shell_command_count(void) { return COMMAND_COUNT; }

const struct shell_command *shell_command_at(int index) {
    if (index < 0 || index >= COMMAND_COUNT) return NULL;
    return g_commands[index];
}

void shell_putc(char c) {
    char buffer[2] = { c, '\0' };
    os_puts(buffer);
    fbcon_putc(c);
}

void shell_puts(const char *text) {
    if (!text) return;
    os_puts(text);
    fbcon_puts(text);
}

void shell_putu32(uint32_t value) {
    char buffer[11];
    int pos = 10;
    buffer[pos] = '\0';
    if (value == 0) {
        buffer[--pos] = '0';
    } else {
        while (value > 0 && pos > 0) {
            buffer[--pos] = (char)('0' + (value % 10u));
            value /= 10u;
        }
    }
    shell_puts(&buffer[pos]);
}

void shell_puthex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buffer[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
    }
    buffer[18] = '\0';
    shell_puts(buffer);
}

/*
 * Prompt. Nel tema classico sta su una riga sua, con il cursore sulla riga
 * sotto: e' il modo in cui si presentava la macchina originale, e cosi' la
 * schermata di avvio e la console diventano la stessa cosa. Il testo lo porta
 * il tema ("READY." sul C64, "1>" sulla shell dell'Amiga).
 */
static void shell_prompt(void) {
    const char *prompt = theme_current()->prompt;
    if (theme_is_classic()) {
        os_puts("\n");
        os_puts(prompt);
        os_puts("\n");
        fbcon_puts_accent("\n");
        fbcon_puts_accent(prompt);
        fbcon_puts_accent("\n");
    } else {
        os_puts(prompt);
        fbcon_puts_accent(prompt);
    }
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int wait_char(void);

/*
 * Le frecce muovono il puntatore hardware.
 *
 * La shell e' il posto giusto per questo: quando aspetta l'input non c'e'
 * nient'altro da fare, e il puntatore serve proprio li'. Cosi' si guida il
 * cursore prima che il mouse esista, e il mouse — quando arrivera' — guidera'
 * lo stesso puntatore: cambia chi lo muove, non cosa si muove.
 */
#define CURSOR_STEP 8

static void arrow_move_cursor(int key) {
    uint16_t x = 0, y = 0;
    int32_t new_x, new_y;

    if (!cursor_is_ready() && cursor_init() != CURSOR_OK) return;

    cursor_position(&x, &y);
    new_x = (int32_t)x;
    new_y = (int32_t)y;
    if (key == USB_KEY_UP)         new_y -= CURSOR_STEP;
    else if (key == USB_KEY_DOWN)  new_y += CURSOR_STEP;
    else if (key == USB_KEY_LEFT)  new_x -= CURSOR_STEP;
    else                           new_x += CURSOR_STEP;
    cursor_move(new_x, new_y);
}

static const struct shell_command *find_command(const char *name) {
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (str_eq(g_commands[i]->name, name)) return g_commands[i];
    }
    return NULL;
}

static int read_line(char *buffer, int max) {
    int length = 0;
    for (;;) {
        int c = wait_char();
        if (c == USB_KEY_UP || c == USB_KEY_DOWN ||
            c == USB_KEY_LEFT || c == USB_KEY_RIGHT) {
            arrow_move_cursor(c);
            continue;
        }
        if (c == '\r' || c == '\n') {
            shell_puts("\n");
            buffer[length] = '\0';
            return length;
        }
        if (c == 8 || c == 127) {          /* backspace / delete */
            if (length > 0) {
                length--;
                shell_puts("\b \b");
            }
            continue;
        }
        if (c >= 32 && c < 127 && length < max - 1) {
            buffer[length++] = (char)c;
            shell_putc((char)c);
        }
    }
}

/*
 * Attende un carattere da una delle due sorgenti: tastiera USB oppure seriale.
 * La seriale si legge senza bloccare (UART diretta), cosi' le due vie non si
 * escludono a vicenda. Quando non c'e' nulla da fare si dorme su wfi.
 */
static int wait_char(void) {
    for (;;) {
        int usb = usb_keyboard_getchar();
        if (usb >= 0) return usb;

        char serial = console_trygetchar();
        if (serial) return (int)(unsigned char)serial;

        fbcon_cursor_blink();
        asm volatile("wfi");
    }
}

static int tokenize(char *line, char **argv, int max) {
    int count = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') { *p++ = '\0'; }
        if (!*p) break;
        if (count < max) argv[count++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return count;
}

void shell_run(void) {
    static char line[LINE_MAX];
    static char *argv[ARG_MAX];

    /* Nel tema classico non si annuncia nulla: dopo il banner compare
     * direttamente il prompt, come sulla macchina originale. */
    if (!theme_is_classic()) {
        shell_puts("\nHomeOS shell - type 'help' for the command list\n");
    }

    for (;;) {
        shell_prompt();
        int length = read_line(line, LINE_MAX);
        if (length == 0) continue;

        int argc = tokenize(line, argv, ARG_MAX);
        if (argc == 0) continue;

        const struct shell_command *command = find_command(argv[0]);
        if (!command) {
            shell_puts("unknown command: ");
            shell_puts(argv[0]);
            shell_puts("   (try 'help')\n");
            continue;
        }
        command->run(argc, argv);
    }
}
