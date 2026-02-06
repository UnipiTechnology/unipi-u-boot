// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */

#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/io.h>
#include <env.h>
#include <i2c.h>
#include <i2c_eeprom.h>
#include <linux/delay.h>
#include <miiphy.h>
#include <netdev.h>
#include <usb.h>

#include "../common/uniee_values.h"

DECLARE_GLOBAL_DATA_PTR;

int ft_unipi_board_setup(void *blob, struct bd_info  *bd);

#if IS_ENABLED(CONFIG_FEC_MXC)
static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&gpr->gpr[1], 0x2000, 0);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	return USB_INIT_HOST;
	//return USB_INIT_DEVICE;
}

int board_phys_sdram_size(phys_size_t *size)
{
	u32 ddr_size = readl(MCU_BOOTROM_BASE_ADDR);

	if (ddr_size == 4) {
		*size = 0x100000000;
	} else if (ddr_size == 3) {
		*size = 0xc0000000;
	} else if (ddr_size == 2) {
		*size = 0x80000000;
	} else if (ddr_size == 1) {
		*size = 0x40000000;
	} else {
		printf("Unknown DDR type!!!\n");
		*size = 0x40000000;
	}
	return 0;
}

int board_init(void)
{
	if (IS_ENABLED(CONFIG_FEC_MXC))
		setup_fec();

	return 0;
}

#if ! IS_ENABLED(CONFIG_XPL_BUILD)
#if IS_ENABLED(CONFIG_ID_EEPROM)

#define RTS_BUTTON (IMX_GPIO_NR(4, 27))
#define PATRON_BUTTON (IMX_GPIO_NR(4, 28))
int rs485_enable = 0;
int tx_active  = 0;

#define UCR2_RXEN	BIT(1)  /* Receiver enabled */
#define USR2_TXDC	BIT(3)  /* Transmitter complete */

void rs485_tx_op(u32 *cr2, u32 *sr2, int op)
{
	//if (!rs485_enable) return;

	if (rs485_enable == 1) {
	if (op == 0) {
		if (tx_active) {
			int i = 0;
			while (!(readl(sr2) & USR2_TXDC)) {
				if (++i == 0x1000000) break;
			}
			gpio_set_value(RTS_BUTTON, 0);
			writel(readl(cr2) | UCR2_RXEN, cr2);
			tx_active = 0;
		}
	} else if (op == 1) {
		if (!tx_active) {
			writel(readl(cr2) & ~((u32) UCR2_RXEN), cr2);
			gpio_set_value(RTS_BUTTON, 1);
			tx_active = 1;
		}
	} else if (op == 2) {
		udelay(1000);
	}
	}
}

int check_button_status(int button_type)
{
	u32 button;
	gpio_request(RTS_BUTTON, "rts button");
	//printf("BUTTON type:%d\n", button_type);
	switch (button_type) {
		case UNIEE_FIELD_VALUE_BUTTON_IRIS:
			gpio_direction_input(RTS_BUTTON);
			/* check pin GPIO4_27 (UART3_RTS) - button + RTS on IRIS model, RTS only on Patron */
			mdelay(50);
			button = RTS_BUTTON;
			break;
		case UNIEE_FIELD_VALUE_BUTTON_PATRON:
			gpio_request(PATRON_BUTTON, "patron button");
			gpio_direction_input(PATRON_BUTTON);
			button = PATRON_BUTTON;
			break;
		default:
			/* unknown button */
			return 0;
	}
//	printf("VAL=%d\n",gpio_get_value(RTS_BUTTON));
	if (gpio_get_value(button) == 0)
		return 0;

	gpio_direction_output(RTS_BUTTON,0);
	tx_active = 0;
	rs485_enable = 1;
	return 1;
}
#endif

int board_late_init(void)
{
	if (gd->ram_size > SZ_1G) {
		env_set("boot_a_script", "run boot_c_script");
	}
	board_late_mmc_env_init();

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "UNIPI");
	env_set("board_rev", "ZULU");
#endif
	return 0;
}

int ft_board_setup(void *blob, struct bd_info  *bd)
{
	char tmpstr[20];
	if (gd->ram_size > SZ_1G) {
		snprintf(tmpstr, sizeof(tmpstr)-1, "zulu 2.0");
	} else {
		snprintf(tmpstr, sizeof(tmpstr)-1, "zulu 1.0");
	}
	tmpstr[sizeof(tmpstr)-1]= '\0';
	fdt_setprop(blob, 0, "altboot-hwrevision", tmpstr, strlen(tmpstr));
	return ft_unipi_board_setup(blob, bd);
}

int identify_usb_hub(struct usb_hub_device *hub, int port)
{
	u32 reg;

	if (!hub || (hub->hub_depth != 0) || (port!=1)) 
		return 0;
	if (! dev_has_ofnode(hub->pusb_dev->controller_dev))
		return 0;
	if (ofnode_read_u32(dev_ofnode(hub->pusb_dev->controller_dev), "reg", &reg))
		return 0;
	if (reg != 0x32e50000)
		return 0;
	return 1;
}

#endif   /* XPL_BUILD */
