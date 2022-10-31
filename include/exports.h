#ifndef __EXPORTS_H__
#define __EXPORTS_H__

#include <irq_func.h>

#ifndef __ASSEMBLY__
#ifdef CONFIG_PHY_AQUANTIA
#include <env.h>
#include <phy_interface.h>
#endif

#ifdef CONFIG_ARCH_ZYNQMP
#include <asm/armv8/mmu.h>
#endif

#include <irq_func.h>

struct spi_slave;

/* These are declarations of exported functions available in C code */
unsigned long get_version(void);
int  getc(void);
int  tstc(void);
void putc(const char);
void puts(const char*);
int printf(const char* fmt, ...);
void install_hdlr(int, interrupt_handler_t, void*);
void free_hdlr(int);
void *malloc(size_t);
#if !CONFIG_IS_ENABLED(SYS_MALLOC_SIMPLE)
void free(void*);
#endif
void __udelay(unsigned long);
unsigned long get_timer(unsigned long);
int vprintf(const char *, va_list);
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);
int strict_strtoul(const char *cp, unsigned int base, unsigned long *res);
char *env_get(const char *name);
int env_set(const char *varname, const char *value);
long simple_strtol(const char *cp, char **endp, unsigned int base);
int strcmp(const char *cs, const char *ct);
unsigned long ustrtoul(const char *cp, char **endp, unsigned int base);
unsigned long long ustrtoull(const char *cp, char **endp, unsigned int base);
#if defined(CONFIG_CMD_I2C) && !defined(CONFIG_DM_I2C)
int i2c_write (uchar, uint, int , uchar* , int);
int i2c_read (uchar, uint, int , uchar* , int);
#endif
#ifdef CONFIG_PHY_AQUANTIA
struct mii_dev *mdio_get_current_dev(void);
struct phy_device *phy_find_by_mask(struct mii_dev *bus, unsigned phy_mask,
		phy_interface_t interface);
struct phy_device *mdio_phydev_for_ethname(const char *ethname);
int miiphy_set_current_dev(const char *devname);
#endif
#ifdef CONFIG_ARCH_ZYNQMP
int xilinx_pm_request(u32, u32, u32, u32, u32, u32*);
void* memcpy(void*, const void*, size_t);
void flush_dcache_all(void);
void invalidate_dcache_range(unsigned long start, unsigned long stop);
void flush_dcache_range(unsigned long start, unsigned long stop);
u64 get_tcr(int el, u64 *pips, u64 *pva_bits);
void mmu_setup_new_map(struct mm_region *map);
#endif

void app_startup(char * const *);

#endif    /* ifndef __ASSEMBLY__ */

struct jt_funcs {
#define EXPORT_FUNC(impl, res, func, ...) res(*func)(__VA_ARGS__);
#include <_exports.h>
#undef EXPORT_FUNC
};


#define XF_VERSION	9

#if defined(CONFIG_X86)
extern gd_t *global_data;
#endif

#endif	/* __EXPORTS_H__ */
