#include <asm/sbi.h>

#define SERVE_EXT_PM  0x09000000
#define PM_MMIO_WRITE 19
#define PM_MMIO_READ  20

int zynqmp_mmio_read(const u32 address, u32 *value)
{
	SBI_CALL(SERVE_EXT_PM, PM_MMIO_READ, address, 0, 0, 0);
	*value = res.a0 >> 32;
	return (int)res.a0;
}

int zynqmp_mmio_write(const u32 address,
		      const u32 mask,
		      const u32 value)
{
	SBI_CALL(SERVE_EXT_PM, PM_MMIO_WRITE, address, mask, value, 0);
	return (int)res.a0;
}
