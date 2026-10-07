#include "../mmio.h"
#include "gpio.h"

#define SYS_IOMUX_BASE 0x13040000UL

#define REG_DOEN    0x000UL
#define REG_DOUT    0x040UL
#define REG_GPI     0x080UL
#define REG_DIN_LOW 0x118UL
#define REG_DIN_HIGH 0x11cUL
#define REG_PAD0    0x120UL   /* pin 0..74   */
#define REG_PAD89   0x284UL   /* pin 89..94  */

#define PAD_INPUT_ENABLE 0x01u

static void update(uintptr_t address, uint32_t mask, uint32_t value) {
    mmio_write32(address, (mmio_read32(address) & ~mask) | (value & mask));
}

/* I registri DOEN/DOUT/GPI coprono quattro pin ciascuno, con un campo da 8 bit
 * per pin: da qui l'indirizzo e lo scostamento del campo. */
static uintptr_t group_addr(uint32_t reg, uint32_t pin) {
    return SYS_IOMUX_BASE + reg + (pin & ~3u);
}

static uint32_t field_shift(uint32_t pin) {
    return (pin & 3u) * 8u;
}

static uintptr_t pad_addr(uint32_t pin) {
    if (pin <= 74u) return SYS_IOMUX_BASE + REG_PAD0 + pin * 4u;
    if (pin >= 89u && pin <= 94u) return SYS_IOMUX_BASE + REG_PAD89 + (pin - 89u) * 4u;
    return 0;
}

int gpio_set_output(uint32_t pin, int value) {
    if (pin > GPIO_MAX_PIN) return GPIO_ERR_PIN;
    uint32_t shift = field_shift(pin);

    /* DOEN = 0: il pin e' pilotato. DOUT = 0/1: livello basso/alto. */
    update(group_addr(REG_DOEN, pin), 0x3fu << shift, 0);
    update(group_addr(REG_DOUT, pin), 0x7fu << shift,
           (value ? 1u : 0u) << shift);

    /* Si tiene abilitato anche il buffer d'ingresso: cosi' il livello del pin
     * si puo' rileggere da DIN, e il self-test ha senso. */
    uintptr_t pad = pad_addr(pin);
    if (pad) update(pad, PAD_INPUT_ENABLE, PAD_INPUT_ENABLE);
    return GPIO_OK;
}

int gpio_set_input(uint32_t pin) {
    if (pin > GPIO_MAX_PIN) return GPIO_ERR_PIN;
    uint32_t shift = field_shift(pin);

    /* DOEN = 1: ingresso. Serve anche il buffer d'ingresso nel pad. */
    update(group_addr(REG_DOEN, pin), 0x3fu << shift, 1u << shift);

    uintptr_t pad = pad_addr(pin);
    if (pad) update(pad, PAD_INPUT_ENABLE, PAD_INPUT_ENABLE);
    return GPIO_OK;
}

int gpio_read(uint32_t pin) {
    if (pin > GPIO_MAX_PIN) return GPIO_ERR_PIN;
    uintptr_t reg = SYS_IOMUX_BASE + ((pin < 32u) ? REG_DIN_LOW : REG_DIN_HIGH);
    return (int)((mmio_read32(reg) >> (pin & 31u)) & 1u);
}

int gpio_is_output(uint32_t pin) {
    if (pin > GPIO_MAX_PIN) return GPIO_ERR_PIN;
    uint32_t value = mmio_read32(group_addr(REG_DOEN, pin));
    uint32_t field = (value >> field_shift(pin)) & 0x3fu;
    return field == 0u;   /* DOEN = 0 significa uscita */
}

int gpio_self_test(uint32_t pin) {
    if (pin > GPIO_MAX_PIN) return GPIO_ERR_PIN;

    if (gpio_set_output(pin, 0) != GPIO_OK) return GPIO_ERR_PIN;
    int low = gpio_read(pin);

    if (gpio_set_output(pin, 1) != GPIO_OK) return GPIO_ERR_PIN;
    int high = gpio_read(pin);

    return (low == 0 && high == 1);
}
