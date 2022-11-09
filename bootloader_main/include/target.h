/*
 *  Copyright (C) 2017, Linaro Ltd
 *  Copyright (c) 2019, Arm Limited
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

#if defined(MCUBOOT_TARGET_CONFIG)
/*
 * Target-specific definitions are permitted in legacy cases that
 * don't provide the information via DTS, etc.
 */
#include MCUBOOT_TARGET_CONFIG
#else
/*
 * Otherwise, the Zephyr SoC header and the DTS provide most
 * everything we need.
 */
#include <soc.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

/*
 * Sanity check the target support.
 */
#if (!defined(CONFIG_XTENSA) && !DT_HAS_CHOSEN(zephyr_flash_controller)) ||    \
    (defined(CONFIG_XTENSA) && !DT_NODE_EXISTS(DT_INST(0, jedec_spi_nor))) ||  \
    !defined(FLASH_ALIGN) || !(FLASH_AREA_LABEL_EXISTS(image_0)) ||            \
    !(FLASH_AREA_LABEL_EXISTS(image_1) || CONFIG_SINGLE_APPLICATION_SLOT) ||   \
    (defined(CONFIG_BOOT_SWAP_USING_SCRATCH) &&                                \
     !FLASH_AREA_LABEL_EXISTS(image_scratch))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#if (MCUBOOT_IMAGE_NUMBER == 2) && (!(FLASH_AREA_LABEL_EXISTS(image_2)) ||     \
                                    !(FLASH_AREA_LABEL_EXISTS(image_3)))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#endif /* H_TARGETS_TARGET_ */
