#include "../command.h"
#include "../shell.h"
#include "../../gpio/gpio.h"

static int parse_u32(const char *text, uint32_t *out) {
    uint32_t value = 0;
    int digits = 0;
    while (text[digits] >= '0' && text[digits] <= '9') {
        value = value * 10u + (uint32_t)(text[digits] - '0');
        digits++;
    }
    if (digits == 0) return 0;
    *out = value;
    return 1;
}

static void print_pin(uint32_t pin) {
    shell_puts("pin ");
    shell_putu32(pin);
    shell_puts(": ");
    int output = gpio_is_output(pin);
    shell_puts(output ? "output" : "input");
    shell_puts(", level ");
    int level = gpio_read(pin);
    shell_puts(level == 1 ? "high" : "low");
    shell_puts("\n");
}

/*
 * Connettore a 40 pin della Milk-V Mars (fonte: documentazione ufficiale
 * Milk-V). NON e' la numerazione del Raspberry Pi: il pin fisico 3 qui e'
 * GPIO58, sul Pi e' GPIO2. Solo i pin con un GPIO associato sono elencati.
 */
static const struct { uint8_t header_pin; uint8_t gpio; } g_header[] = {
    { 3, 58}, { 5, 57}, { 7, 55}, { 8,  5}, {10,  6}, {11, 42}, {12, 38},
    {13, 43}, {15, 47}, {16, 54}, {18, 51}, {19, 52}, {21, 53}, {22, 50},
    {23, 48}, {24, 49}, {26, 56}, {27, 45}, {28, 40}, {29, 37}, {31, 39},
    {32, 46}, {33, 59}, {35, 63}, {36, 36}, {37, 60}, {38, 61}, {40, 44},
};

static void print_map(void) {
    shell_puts("40-pin header (Milk-V Mars): physical pin -> GPIO\n");
    for (unsigned i = 0; i < sizeof(g_header) / sizeof(g_header[0]); i++) {
        char line[8];
        int n = 0;
        uint32_t hp = g_header[i].header_pin;
        uint32_t gp = g_header[i].gpio;
        if (hp >= 10) line[n++] = (char)('0' + hp / 10);
        line[n++] = (char)('0' + hp % 10);
        line[n++] = ' ';
        line[n++] = '-';
        line[n++] = '>';
        line[n++] = ' ';
        if (gp >= 10) line[n++] = (char)('0' + gp / 10);
        line[n++] = (char)('0' + gp % 10);
        line[n] = '\0';
        shell_puts("  ");
        shell_puts(line);
        if ((i % 4u) == 3u) shell_puts("\n");
        else shell_puts("   ");
    }
    shell_puts("\nNote: this is NOT the Raspberry Pi numbering.\n");
}

SHELL_COMMAND(cmd_gpio, "gpio", "GPIO: read/out/in/test/map <pin> [0|1]") {
    if (argc >= 2 && argv[1][0] == 'm') {
        print_map();
        return;
    }
    if (argc < 3) {
        shell_puts("usage:\n");
        shell_puts("  gpio read <pin>        read the level\n");
        shell_puts("  gpio out <pin> <0|1>   set the pin as output\n");
        shell_puts("  gpio in <pin>          set the pin as input\n");
        shell_puts("  gpio test <pin>        verify the chain (drive and read back)\n");
        shell_puts("  gpio map               40-pin header map\n");
        shell_puts("valid pins: 0..63. Note: pin 25 is the USB VBUS.\n");
        return;
    }

    uint32_t pin = 0;
    if (!parse_u32(argv[2] ? argv[2] : "", &pin) || pin > GPIO_MAX_PIN) {
        shell_puts("invalid pin (0..63)\n");
        return;
    }

    const char *verb = argv[1];

    if (verb[0] == 'r') {
        print_pin(pin);
        return;
    }

    if (verb[0] == 'o') {
        uint32_t value = 0;
        if (argc < 4 || !parse_u32(argv[3], &value)) {
            shell_puts("usage: gpio out <pin> <0|1>\n");
            return;
        }
        gpio_set_output(pin, value ? 1 : 0);
        print_pin(pin);
        return;
    }

    if (verb[0] == 'i') {
        gpio_set_input(pin);
        print_pin(pin);
        return;
    }

    if (verb[0] == 't') {
        shell_puts("testing pin ");
        shell_putu32(pin);
        shell_puts(": drive low, read back, drive high, read back\n");
        int rc = gpio_self_test(pin);
        if (rc == 1) {
            shell_puts("OK: the level read follows the level driven\n");
        } else if (rc == 0) {
            shell_puts("NOT verified: the level read does not follow the level driven.\n");
            shell_puts("If the pin is used by a peripheral or has a load, that is normal.\n");
        } else {
            shell_puts("invalid pin\n");
        }
        return;
    }

    shell_puts("unknown subcommand (read, out, in, test, map)\n");
}
