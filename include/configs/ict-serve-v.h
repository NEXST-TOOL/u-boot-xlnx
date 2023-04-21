/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2023 Institute of Computing Technology, Chinese Academy of Sciences and its affiliates.
 *
 * Authors:
 *  Yisong Chang <changyisong@ict.ac.cn>
 * 
 */

#ifndef __ICT_SERVE_V_H
#define __ICT_SERVE_V_H

#include <linux/sizes.h>

#define CONFIG_SYS_SDRAM_BASE		0x80000000UL

#define SERVE_SCRIPT_ADDR \
        "scriptaddr=0x80400000\0"

#include <configs/ict-serve.h>

#endif /* __ICT_SERVE_V_H */
