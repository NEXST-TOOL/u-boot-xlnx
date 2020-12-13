// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx XDMA PL host controller driver.
 *
 * Copyright (c) 2020-2021 ICT, CAS.
 * Author: Yisong Chang <changyisong@ict.ac.cn>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <pci.h>
#include <pci_internal.h>
#include <reset.h>
#include <asm/io.h>
#include <linux/iopoll.h>
#include <linux/list.h>

#define XILINX_PCIE_REG_IDR		0x00000138
#define XILINX_PCIE_REG_IMR		0x0000013c
#define XILINX_PCIE_REG_PSCR		0x00000144
#define XILINX_PCIE_REG_RPSC		0x00000148

#define XILINX_PCIE_IMR_ALL_MASK	0x1FF30FED
#define XILINX_PCIE_IDR_ALL_MASK	0xFFFFFFFF

/* Phy Status/Control Register definitions */
#define XILINX_PCIE_REG_PSCR_LNKUP	BIT(11)

/* Root Port Status/control Register definitions */
#define XILINX_PCIE_REG_RPSC_BEN	BIT(0)

/* ECAM definitions */
#define ECAM_BUS_NUM_SHIFT		20
#define ECAM_DEV_NUM_SHIFT		12

struct xilinx_pcie_port {
	void __iomem *reg_base;
	struct list_head list;
	struct xilinx_pcie *pcie;
	u32 slot;
};

struct xilinx_pcie {
	struct list_head ports;
};

static struct xilinx_pcie_port *xilinx_xdma_pl_pcie_find_port(struct udevice *udev, 
    pci_dev_t bdf)
{
	struct xilinx_pcie *pcie = dev_get_priv(udev);
  struct xilinx_pcie_port *port;
  struct udevice *child, *parent;
  struct pci_child_platdata *pplat;
  pci_dev_t dev_bdf;
  int ret;

  /* Get current bus of the required device */
  ret = pci_get_bus(PCI_BUS(bdf), &parent);
  if (ret == -ENODEV)
    return NULL;

  /* Get the PCI bdf number if current bus is not the root bus */
  if (PCI_BUS(bdf) != 0)
  {
    pplat = dev_get_parent_platdata(parent);
    dev_bdf = pplat->devfn;
    child = parent;
  }
  else
    dev_bdf = bdf;

  while(PCI_BUS(dev_bdf) != 0)
  {
    parent = child->parent;
    dev_bdf = dm_pci_get_bdf(parent);
    child = parent;
  }

  list_for_each_entry(port, &pcie->ports, list)
  {
    if (port->slot == PCI_DEV(dev_bdf))
      return port;
  }

  return NULL;
}

static int xilinx_xdma_pl_pcie_config_address(struct udevice *udev, pci_dev_t bdf,
				   uint offset, void **paddress)
{
  struct xilinx_pcie_port *rp;
  unsigned int relbus;
  int ret;

  rp = xilinx_xdma_pl_pcie_find_port(udev, bdf);
  if (rp == NULL)
    return -ENODEV;

  relbus = (PCI_BUS(bdf) << ECAM_BUS_NUM_SHIFT) | 
    ((PCI_BUS(bdf) ? ((bdf >> 8) & 0xff) : 0) << ECAM_DEV_NUM_SHIFT);

	*paddress = rp->reg_base + relbus + offset;

	return 0;
}

static int xilinx_xdma_pl_pcie_read_config(struct udevice *bus, pci_dev_t bdf,
				uint offset, ulong *valuep,
				enum pci_size_t size)
{
	return pci_generic_mmap_read_config(bus, xilinx_xdma_pl_pcie_config_address,
					    bdf, offset, valuep, size);
}

static int xilinx_xdma_pl_pcie_write_config(struct udevice *bus, pci_dev_t bdf,
				 uint offset, ulong value,
				 enum pci_size_t size)
{
	return pci_generic_mmap_write_config(bus, xilinx_xdma_pl_pcie_config_address,
					     bdf, offset, value, size);
}

static const struct dm_pci_ops xilinx_xdma_pl_pcie_ops = {
	.read_config	= xilinx_xdma_pl_pcie_read_config,
	.write_config	= xilinx_xdma_pl_pcie_write_config,
};

static void xilinx_xdma_pl_pcie_port_free(struct xilinx_pcie_port *port)
{
	list_del(&port->list);
	free(port);
}

static bool xilinx_xdma_pl_pcie_link_up(struct xilinx_pcie_port *port)
{
	uint32_t pscr = readl(port->reg_base + XILINX_PCIE_REG_PSCR);

	return pscr & XILINX_PCIE_REG_PSCR_LNKUP;
}

static int xilinx_xdma_pl_pcie_startup_port(struct xilinx_pcie_port *port)
{
	u32 val;
	int err;

  /* check if PCIe port is linked up */
  err = xilinx_xdma_pl_pcie_link_up(port);
  if (!err)
    return -ENODEV;

	/* disable interrupt */
  writel(~XILINX_PCIE_IDR_ALL_MASK, port->reg_base + XILINX_PCIE_REG_IMR);

  /* clear pending interrupts */
  val = readl(port->reg_base + XILINX_PCIE_REG_IDR);
  writel(val & XILINX_PCIE_IMR_ALL_MASK, port->reg_base + XILINX_PCIE_REG_IDR);

  /* Enable the bridge enable bit */
  val = readl(port->reg_base + XILINX_PCIE_REG_RPSC);
  writel(val | XILINX_PCIE_REG_RPSC_BEN, port->reg_base + XILINX_PCIE_REG_RPSC);

	return 0;
}

static void xilinx_xdma_pl_pcie_enable_port(struct xilinx_pcie_port *port)
{
	if (!xilinx_xdma_pl_pcie_startup_port(port))
		return;

	pr_err("Port %d link down\n", port->slot);
exit:
	xilinx_xdma_pl_pcie_port_free(port);
}

static int xilinx_xdma_pl_pcie_parse_port(struct udevice *dev, u32 slot)
{
	struct xilinx_pcie *pcie = dev_get_priv(dev);
	struct xilinx_pcie_port *port;
	char name[10];
	int err;

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	snprintf(name, sizeof(name), "rp%d", slot);
	port->reg_base = dev_remap_addr_name(dev, name);
	if (!port->reg_base)
		return -ENOENT;

	port->slot = slot;
	port->pcie = pcie;

	INIT_LIST_HEAD(&port->list);
	list_add_tail(&port->list, &pcie->ports);

	return 0;
}

static int xilinx_xdma_pl_pcie_probe(struct udevice *dev)
{
	struct xilinx_pcie *pcie = dev_get_priv(dev);
	struct xilinx_pcie_port *port, *tmp;
	ofnode subnode;
	int err;

	INIT_LIST_HEAD(&pcie->ports);

	dev_for_each_subnode(subnode, dev) {
		struct fdt_pci_addr addr;
		u32 slot = 0;

		if (!ofnode_is_available(subnode))
			continue;

		err = ofnode_read_pci_addr(subnode, 0, "reg", &addr);
		if (err)
			return err;

		slot = PCI_DEV(addr.phys_hi);

		err = xilinx_xdma_pl_pcie_parse_port(dev, slot);
		if (err)
			return err;
	}

	/* enable each port, and then check link status */
	list_for_each_entry_safe(port, tmp, &pcie->ports, list)
		xilinx_xdma_pl_pcie_enable_port(port);

	return 0;
}

static const struct udevice_id xilinx_xdma_pl_pcie_ids[] = {
	{ .compatible = "xlnx,xdma-host-3.00", },
	{ }
};

U_BOOT_DRIVER(pcie_xdma_pl) = {
	.name	= "pcie_xdma_pl",
	.id	= UCLASS_PCI,
	.of_match = xilinx_xdma_pl_pcie_ids,
	.ops	= &xilinx_xdma_pl_pcie_ops,
	.probe	= xilinx_xdma_pl_pcie_probe,
	.priv_auto_alloc_size = sizeof(struct xilinx_pcie),
};
