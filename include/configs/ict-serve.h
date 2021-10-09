/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences and its affiliates.
 *
 * Authors:
 *  Yisong Chang <changyisong@ict.ac.cn>
 * 
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#ifdef CONFIG_SERVE_PLAT_R
#define CONFIG_SYS_SDRAM_BASE		0x50000000UL
#elif CONFIG_SERVE_PLAT_H
#define CONFIG_SYS_SDRAM_BASE		0x80000000UL
#else
#define CONFIG_SYS_SDRAM_BASE		0xB0000000UL
#endif

#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + SZ_4M)

#define CONFIG_SYS_LOAD_ADDR		(CONFIG_SYS_SDRAM_BASE + SZ_2M)

#define CONFIG_SYS_MALLOC_LEN		SZ_8M

#define CONFIG_SYS_BOOTM_LEN		SZ_16M

#ifdef CONFIG_SERVE_PLAT_R
#define SERVE_IMAGE_ADDR \
	"kernel_addr=0x50600000\0" \
        "fdt_addr=0x52500000\0" \
        "loadbootenv_addr=0x50400000\0"
#elif CONFIG_SERVE_PLAT_H
#define SERVE_IMAGE_ADDR \
	"kernel_addr_r=0x80600000\0" \
        "fdt_addr_r=0x82500000\0" \
        "scriptaddr=0x80400000\0"
#else
#define SERVE_IMAGE_ADDR \
	"kernel_addr_r=0xB0600000\0" \
	"fdt_addr_r=0xB2500000\0" \
        "scriptaddr=0xB0400000\0"
#endif

#define SERVE_FDT_HIGH \
        "fdt_high=0xFFFFFFFFFFFFFFFF\0"

#if defined(CONFIG_CMD_DHCP)
# define BOOT_TARGET_DEVICES_DHCP(func)	func(DHCP, dhcp, na)
#else
# define BOOT_TARGET_DEVICES_DHCP(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_DEVICES_DHCP(func)

#include <config_distro_bootcmd.h>

#ifndef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
  SERVE_IMAGE_ADDR \
  SERVE_FDT_HIGH
#endif

#endif /* __CONFIG_H */
