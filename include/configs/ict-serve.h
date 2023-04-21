/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences and its affiliates.
 *
 * Authors:
 *  Yisong Chang <changyisong@ict.ac.cn>
 * 
 */

#ifndef __ICT_SERVE_H
#define __ICT_SERVE_H

#include <linux/sizes.h>

#define CONFIG_SYS_INIT_SP_ADDR        (CONFIG_SYS_SDRAM_BASE + SZ_4M)

#define CONFIG_SYS_LOAD_ADDR           (CONFIG_SYS_SDRAM_BASE + SZ_2M)

#define CONFIG_SYS_MALLOC_LEN          SZ_8M

#define CONFIG_SYS_BOOTM_LEN           SZ_16M

#define SERVE_FDT_INITRD_HIGH \
        "fdt_high=0xFFFFFFFFFFFFFFFF\0" \
        "initrd_high=0xFFFFFFFFFFFFFFFF\0"

#if defined(CONFIG_CMD_DHCP)
# define BOOT_TARGET_DEVICES_DHCP(func)	func(DHCP, dhcp, na)
#else
# define BOOT_TARGET_DEVICES_DHCP(func)
#endif

#if defined(CONFIG_MMC_SDHCI_ZYNQ)
# define BOOT_TARGET_DEVICES_MMC(func) func(MMC, mmc, 0)
#else
# define BOOT_TARGET_DEVICES_MMC(func)
#endif

#if defined(CONFIG_NVME)
# define BOOT_TARGET_DEVICES_NVME(func)	func(NVME, nvme, 0)
#else
# define BOOT_TARGET_DEVICES_NVME(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_DEVICES_MMC(func) \
	BOOT_TARGET_DEVICES_NVME(func) \
	BOOT_TARGET_DEVICES_DHCP(func) \

#include <config_distro_bootcmd.h>

#ifndef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
  SERVE_SCRIPT_ADDR \
  SERVE_FDT_INITRD_HIGH \
  BOOTENV
#endif

#endif /* __ICT_SERVE_H */
