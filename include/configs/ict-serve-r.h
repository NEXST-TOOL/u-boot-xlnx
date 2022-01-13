/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2019 Institute of Computing Technology, Chinese Academy of Sciences and its affiliates.
 *
 * Authors:
 *  Yisong Chang <changyisong@ict.ac.cn>
 * 
 */

#ifndef __ICT_SERVE_R_H
#define __ICT_SERVE_R_H

#include <linux/sizes.h>

#define CONFIG_SYS_SDRAM_BASE		0x50000000UL

#define SERVE_SCRIPT_ADDR \
        "scriptaddr=0x50400000\0"

#include <configs/ict-serve.h>

#endif /* __ICT_SERVE_R_H */
