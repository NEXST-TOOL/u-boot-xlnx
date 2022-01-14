// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <exports.h>
#include <sha3.h>

#if defined(CONFIG_ARCH_ZYNQMP)
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

#define PM_SIP_SVC 0xC2000000
#define PM_SIP_SVC_MASK 0xFFFF0000
#define PM_FPGA_LOAD 22

#define ARM_ASSIST_FPGA_LOAD 0xC2100016

#define BITSTREAM_BUFFER  0x00080000

#else
static volatile u32 *rv_reset_reg = (void*)0x43c00000;
#endif

int serve_daemon ()
{
  /* Print the ABI version */
  printf ("Example expects ABI version %d\n", XF_VERSION);
  printf ("Actual U-Boot ABI version %d\n", (int)get_version());

#if defined(CONFIG_ARCH_ZYNQMP)  
  *ipc_req = 0;
#endif

  *rv_reset_reg = 0;

  for (;;) {
#if defined(CONFIG_ARCH_ZYNQMP)
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
    else if ((api & PM_SIP_SVC_MASK) == PM_SIP_SVC) {
      xilinx_pm_request(api, arg0, arg1, arg2, arg3, ipc_ret);
    }
    else if (api == ARM_ASSIST_FPGA_LOAD) {
      void *src, *dst;
      u32 size;
      u64 addr;

      // copy bitstream to a buffer in DRAM accessible by CSU DMA
      addr = (arg0 | ((u64)arg1 << 32));
      src = (void *)addr;
      dst = (void *)BITSTREAM_BUFFER;
      size = arg2 << 2; // arg2 is the bitstream size in 32-bit words

      invalidate_dcache_range(addr, addr+size);
      memcpy(dst, src, size);
      flush_dcache_range(BITSTREAM_BUFFER, BITSTREAM_BUFFER+size);

      xilinx_pm_request(PM_SIP_SVC | PM_FPGA_LOAD, BITSTREAM_BUFFER, 0, arg2, arg3, ipc_ret);
    }
    else {
      printf("Unknown API 0x%08x\n", api);
    }

    *ipc_req = 0;
#endif
  }

  return 0;
}
