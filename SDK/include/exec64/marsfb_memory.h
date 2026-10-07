#ifndef EXEC64_MARSFB_MEMORY_H
#define EXEC64_MARSFB_MEMORY_H
/* Board reservation shared by the Core mapper and the supervisor device.
 * Never expose this mapping to user tasks or write through a cached alias. */
#define MARSFB_PHYS 0x70000000ULL
#define MARSFB_UNCACHED 0x470000000ULL
#define MARSFB_MAX_WIDTH 1920u
#define MARSFB_MAX_HEIGHT 1080u
#define MARSFB_BYTES (MARSFB_MAX_WIDTH * MARSFB_MAX_HEIGHT * 4u)
#endif
