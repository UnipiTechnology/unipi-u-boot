// SPDX-License-Identifier: GPL-2.0+
/*
 * Rockchip on-chip xHCI glue, originally for rk3399.
 *
 * Copyright (c) 2016 Rockchip, Inc.
 * Authors: Daniel Meng <daniel.meng@rock-chips.com>
 *          Kever Yang <kever.yang@rock-chips.com>
 *   https://lists.denx.de/pipermail/u-boot/2016-August/264107.html
 *
 * RK3328 port (2023): Miroslav Ondra <ondra@faster.cz>
 *
 * This is a minimal vendor driver that drives the
 * DWC3 core directly and configures the USB2 PHY registers by hand. Unlike the
 * generic drivers/usb/host/xhci-dwc3.c path, it does NOT enable the USB3OTG
 * clocks through the clock framework and does NOT invoke the (absent on RK3328)
 * SuperSpeed PHY driver. On RK3328 the InnoSilicon USB3 PHY is bugged hardware:
 * touching its clock/suspend state or running the generic DWC3 PHY setup makes
 * SuperSpeed enumeration marginal. Leaving the clocks in their CRU default-on
 * state and only touching the UTMI (USB2) registers is what makes USB3 thumb
 * drives enumerate reliably on this SoC.
 *
 * Ported from the Unipi G1 v2023.04 patch set to the v2026.01 driver model.
 */

#include <dm.h>
#include <log.h>
#include <linux/delay.h>
#include <usb.h>
#include <usb/xhci.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/usb/dwc3.h>
#include <power/regulator.h>

struct rockchip_xhci_plat {
	fdt_addr_t hcd_base;
	struct udevice *vbus_supply;
};

/*
 * Contains pointers to register base addresses
 * for the usb controller.
 */
struct rockchip_xhci {
	struct usb_plat usb_plat;
	struct xhci_ctrl ctrl;
	struct xhci_hccr *hcd;
	struct dwc3 *dwc3_reg;
};

static int xhci_usb_of_to_plat(struct udevice *dev)
{
	struct rockchip_xhci_plat *plat = dev_get_plat(dev);
	int ret;

	/* Get the base address for XHCI controller from the device node */
	plat->hcd_base = dev_read_addr(dev);
	if (plat->hcd_base == FDT_ADDR_T_NONE) {
		pr_err("Can't get the XHCI register base address\n");
		return -ENXIO;
	}

	/* Vbus regulator (optional) */
	ret = device_get_supply_regulator(dev, "vbus-supply",
					   &plat->vbus_supply);
	if (ret)
		debug("Can't get VBus regulator!\n");

	return 0;
}

/*
 * rockchip_dwc3_phy_setup() - Configure USB PHY Interface of DWC3 Core
 * @dwc3_reg: Pointer to the DWC3 core registers
 * @dev: Pointer to the udevice (for DT properties)
 *
 * Only the UTMI (USB2) PHY is configured here. The SuperSpeed PIPE is left
 * untouched on purpose (see file header).
 */
static void rockchip_dwc3_phy_setup(struct dwc3 *dwc3_reg, struct udevice *dev)
{
	u32 reg;
	u32 utmi_bits;

	/* Set dwc3 usb2 phy config */
	reg = readl(&dwc3_reg->g_usb2phycfg[0]);

	if (dev_read_bool(dev, "snps,dis-enblslpm-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;

	utmi_bits = dev_read_u32_default(dev, "snps,phyif-utmi-bits", -1U);
	if (utmi_bits == 16) {
		reg |= DWC3_GUSB2PHYCFG_PHYIF;
		reg &= ~DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
		reg |= DWC3_GUSB2PHYCFG_USBTRDTIM_16BIT;
	} else if (utmi_bits == 8) {
		reg &= ~DWC3_GUSB2PHYCFG_PHYIF;
		reg &= ~DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
		reg |= DWC3_GUSB2PHYCFG_USBTRDTIM_8BIT;
	}

	if (dev_read_bool(dev, "snps,dis-u2-freeclk-exists-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;

	if (dev_read_bool(dev, "snps,dis-u2-susphy-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

	writel(reg, &dwc3_reg->g_usb2phycfg[0]);
}

/*
 * rockchip_dwc3_core_init() - minimal DWC3 core init for RK3328.
 *
 * This is a local replacement for the stock dwc3_core_init() in
 * xhci-dwc3.c. The difference is deliberate and is the whole point of this
 * driver: the stock path runs dwc3_phy_reset(), which asserts the USB3 PIPE
 * PHY soft-reset (DWC3_GUSB3PIPECTL_PHYSOFTRST) for 100 ms and then
 * deasserts it. On RK3328 the SuperSpeed PHY is the bugged InnoSilicon IP:
 * toggling its reset makes SuperSpeed link re-establishment marginal (the
 * drive enumerates only on a fraction of init attempts). The InnoSilicon SS
 * PHY has no mainline driver and is left in its power-on default state, in
 * which it can establish a 5 Gb/s link on its own; resetting it via the PIPE
 * interface only makes things worse.
 *
 * Therefore here we soft-reset only the USB2 (UTMI) PHY and the DWC3 core,
 * and leave g_usb3pipectl[0] untouched. The GCTL setup below is otherwise
 * identical to dwc3_core_init().
 */
static int rockchip_dwc3_core_init(struct dwc3 *dwc3_reg)
{
	u32 reg, revision;
	unsigned int dwc3_hwparams1;

	revision = readl(&dwc3_reg->g_snpsid);
	if ((revision & DWC3_GSNPSID_MASK) != 0x55330000 &&
	    (revision & DWC3_GSNPSID_MASK) != 0x33310000) {
		pr_err("this is not a DesignWare USB3 DRD Core\n");
		return -ENXIO;
	}

	/* Put the core in reset, reset only the USB2 PHY, then release core. */
	setbits_le32(&dwc3_reg->g_ctl, DWC3_GCTL_CORESOFTRESET);
	setbits_le32(&dwc3_reg->g_usb2phycfg, DWC3_GUSB2PHYCFG_PHYSOFTRST);
	mdelay(100);
	clrbits_le32(&dwc3_reg->g_usb2phycfg, DWC3_GUSB2PHYCFG_PHYSOFTRST);
	clrbits_le32(&dwc3_reg->g_ctl, DWC3_GCTL_CORESOFTRESET);

	dwc3_hwparams1 = readl(&dwc3_reg->g_hwparams1);

	reg = readl(&dwc3_reg->g_ctl);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~DWC3_GCTL_DISSCRAMBLE;
	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc3_hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	default:
		debug("No power optimization available\n");
		break;
	}

	if ((revision & DWC3_REVISION_MASK) < 0x190a)
		reg |= DWC3_GCTL_U2RSTECN;

	writel(reg, &dwc3_reg->g_ctl);

	return 0;
}

static int rockchip_xhci_core_init(struct rockchip_xhci *rkxhci,
				   struct udevice *dev)
{
	int ret;

	ret = rockchip_dwc3_core_init(rkxhci->dwc3_reg);
	if (ret) {
		pr_err("failed to initialize core\n");
		return ret;
	}

	rockchip_dwc3_phy_setup(rkxhci->dwc3_reg, dev);

	/* We are hard-coding DWC3 core to Host Mode */
	dwc3_set_mode(rkxhci->dwc3_reg, DWC3_GCTL_PRTCAP_HOST);

	return 0;
}

static int rockchip_xhci_core_exit(struct rockchip_xhci *rkxhci)
{
	return 0;
}

static int xhci_usb_probe(struct udevice *dev)
{
	struct rockchip_xhci_plat *plat = dev_get_plat(dev);
	struct rockchip_xhci *ctx = dev_get_priv(dev);
	struct xhci_hcor *hcor;
	int ret;

	ctx->hcd = (struct xhci_hccr *)plat->hcd_base;
	ctx->dwc3_reg = (struct dwc3 *)((char *)(ctx->hcd) + DWC3_REG_OFFSET);
	hcor = (struct xhci_hcor *)((uint64_t)ctx->hcd +
			HC_LENGTH(xhci_readl(&ctx->hcd->cr_capbase)));

	if (plat->vbus_supply) {
		/*
		 * VBus is optional and may already be on (e.g. regulator-always-on
		 * via vcc_host_5v on the G1). regulator_set_enable_if_allowed()
		 * treats -EALREADY/-EACCES/-ENOSYS/-EBUSY as success so we don't
		 * abort the probe when the supply is already enabled.
		 */
		ret = regulator_set_enable_if_allowed(plat->vbus_supply, true);
		if (ret) {
			pr_err("XHCI: failed to set VBus supply\n");
			return ret;
		}
	}

	ret = rockchip_xhci_core_init(ctx, dev);
	if (ret) {
		pr_err("XHCI: failed to initialize controller\n");
		return ret;
	}

	return xhci_register(dev, ctx->hcd, hcor);
}

static int xhci_usb_remove(struct udevice *dev)
{
	struct rockchip_xhci_plat *plat = dev_get_plat(dev);
	struct rockchip_xhci *ctx = dev_get_priv(dev);
	int ret;

	ret = xhci_deregister(dev);
	if (ret)
		return ret;

	ret = rockchip_xhci_core_exit(ctx);
	if (ret)
		return ret;

	if (plat->vbus_supply) {
		ret = regulator_set_enable_if_allowed(plat->vbus_supply, false);
		if (ret)
			pr_err("XHCI: failed to set VBus supply\n");
	}

	return ret;
}

static const struct udevice_id xhci_usb_ids[] = {
	{ .compatible = "rockchip,rk3328-xhci" },
	{ }
};

U_BOOT_DRIVER(xhci_rockchip) = {
	.name	= "xhci_rockchip",
	.id	= UCLASS_USB,
	.of_match = xhci_usb_ids,
	.of_to_plat = xhci_usb_of_to_plat,
	.probe = xhci_usb_probe,
	.remove = xhci_usb_remove,
	.ops	= &xhci_usb_ops,
	.bind	= dm_scan_fdt_dev,
	.plat_auto = sizeof(struct rockchip_xhci_plat),
	.priv_auto = sizeof(struct rockchip_xhci),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};

/*
 * Inert stub for the RK3328 USB3 PHY node. RK3328 has no mainline SuperSpeed
 * PHY driver (the InnoSilicon IP is bugged and only exists as an unmerged Linux
 * RFC). Binding this no-op driver keeps the device tree valid if a
 * "rockchip,rk3328-usb3-phy" node is ever present without engaging any SS PHY
 * init sequence that would trip the erratum.
 */
static const struct udevice_id usb_phy_ids[] = {
	{ .compatible = "rockchip,rk3328-usb3-phy" },
	{ }
};

U_BOOT_DRIVER(usb_phy_rockchip) = {
	.name = "usb_phy_rockchip",
	.of_match = usb_phy_ids,
};
