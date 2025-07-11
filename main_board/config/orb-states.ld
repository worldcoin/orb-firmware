#include <zephyr/linker/iterable_sections.h>

ITERABLE_SECTION_RAM(orb_state_dynamic, Z_LINK_ITERABLE_SUBALIGN)
ITERABLE_SECTION_ROM(orb_state_const, Z_LINK_ITERABLE_SUBALIGN)
