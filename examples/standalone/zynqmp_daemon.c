// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <exports.h>
#include <sha3.h>

#define RV_ARM_IPC_BASE   0x80000000
#define RV_ARM_IPC_REQ    (RV_ARM_IPC_BASE + 0x0)
#define RV_ARM_IPC_API    (RV_ARM_IPC_BASE + 0x4)
#define RV_ARM_IPC_ARG    (RV_ARM_IPC_BASE + 0x10)
#define RV_ARM_IPC_RET    (RV_ARM_IPC_BASE + 0x20)

static volatile u32 *ipc_req = (void*)RV_ARM_IPC_REQ;
static volatile u32 *ipc_api = (void*)RV_ARM_IPC_API;
static volatile u32 *ipc_arg = (void*)RV_ARM_IPC_ARG;
static volatile u32 *ipc_ret = (void*)RV_ARM_IPC_RET;

static volatile u32 *rv_reset_reg = (void*)0x83c00000;

//IPC function ID
#define SHA3_ARM_ASSIST  0xFFFF0000
#define SHA3_UPDATE  0
#define SHA3_FINALIZE  1

int zynqmp_daemon ()
{
  /* Print the ABI version */
  printf ("Example expects ABI version %d\n", XF_VERSION);
  printf ("Actual U-Boot ABI version %d\n", (int)get_version());

  printf ("RV-ARM IPC Service\n");

  *ipc_req = 0;

  *rv_reset_reg = 0;

  for (;;) {
    u32 api, arg0, arg1, arg2, arg3, arg4;

    while (!*ipc_req);

    api = *ipc_api;
    arg0 = ipc_arg[0];
    arg1 = ipc_arg[1];
    arg2 = ipc_arg[2];
    arg3 = ipc_arg[3];
    arg4 = ipc_arg[4];

    if( (api & SHA3_ARM_ASSIST) == SHA3_ARM_ASSIST ) {
      u64 sha3_ctx_addr = (u64)arg1 << 32;
      sha3_ctx_addr |= arg0;

      api &= 0xFF;

      if (api == SHA3_UPDATE) {
        u64 len = (u64)arg4;

        u64 addr = (u64)arg3 << 32;
        addr |= arg2;

        sha3_update((sha3_ctx_t *)sha3_ctx_addr, (void *)addr, len);
      }

      else if (api == SHA3_FINALIZE)
        sha3_final((sha3_ctx_t *)sha3_ctx_addr);

      else
        printf ("Invalid SHA3 IPC function\n");

      //force contents of sha3_ctx_t available in RISC-V's cache or DRAM
      flush_dcache_all();
    }
    else
      xilinx_pm_request(api, arg0, arg1, arg2, arg3, ipc_ret);

    *ipc_req = 0;
  }

  return 0;
}
