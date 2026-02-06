// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2021 Miroslav Ondra, Faster CZ (Unipi Technology)
 */

#include <clk.h>
#include <dm.h>
#include <led.h>
#include <net.h>
#include <ram.h>
#include <i2c_eeprom.h>
#include <asm-generic/gpio.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch-rockchip/boot_mode.h>
#include <asm/arch-rockchip/clock.h>
#include <asm/arch-rockchip/periph.h>
//#include <asm/arch-rockchip/misc.h>
#include <asm/arch-rockchip/hardware.h>
#include <asm/arch-rockchip/grf_rk3328.h>
#include <asm/arch-rockchip/uart.h>
#include <asm/arch-rockchip/gpio.h>
#include <power/regulator.h>
#include <linux/delay.h>
#include <serial.h>
#include <ns16550.h>
#include <bloblist.h>
#include <rtc.h>
#include <usb.h>

#include "../common/uniee_values.h"

#define CRU_GLB_CNT_TH     0xff440090

struct unipi_handoff {
	u32 ddr;
};

int ft_unipi_board_setup(void *blob, struct bd_info  *bd);

#if (defined(CONFIG_TPL_BOARD_INIT) && defined(CONFIG_TPL_BUILD))
int rk3328_dmc_get_ddr_mode(struct udevice *dev);

void spl_board_init(void)
{
	struct udevice *dev;
	struct unipi_handoff *handoff;
	int ret, mode;

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (!ret) {
		mode = rk3328_dmc_get_ddr_mode(dev);
		ret = bloblist_ensure_size(BLOBLISTT_UNIPI_DDR, sizeof(struct unipi_handoff),
					   0, (void **)&handoff);
		debug("%s: bloblist_ensure ret=%d\n", __func__, ret);
		if (ret==0) {
			handoff->ddr = mode; /* 0:ddr3 1:ddr4 */
			bloblist_finish();
		}
	}
}
#else
void spl_board_init(void)
{
}
#endif


#ifdef CONFIG_MISC_INIT_R

int rockchip_early_misc_init_r(void)
{
	u8 ethaddr[6];
	u8 ethaddr1[6];

	// Both ethaddr and ethaddr1 must be set from eeprom, else clear it to use cpuid# for mac
	if (!eth_env_get_enetaddr("ethaddr", ethaddr) ||
	    !eth_env_get_enetaddr("ethaddr1", ethaddr1)) {
		env_set("ethaddr", NULL);
	}
	return 0;
}
#endif

/*
#define G1_BUTTON    (2*32+ GPIO(BANK_C, 5)) 
//2*8+5)
#define G1_TX_ENABLE (2*32+ GPIO(BANK_C, 2))
//2*8+2)
#define G1_RX_ENABLE (2*32+ GPIO(BANK_A, 2))
//0*8+2)
*/
#define G1_BUTTON    (2*32+ 2*8+5)
#define G1_TX_ENABLE (2*32+ 2*8+2)
#define G1_RX_ENABLE (2*32+ 0*8+2)

static int rs485_enable = 0;

#ifndef CONFIG_SPL_BUILD
void rs485_tx_op(struct udevice *dev, int op)
{
	if (!rs485_enable) return;

	if (op == 0) {
		int i = 0;
		struct dm_serial_ops *ops = serial_get_ops(dev);
		if (ops->pending) {
			//while (ns16550_serial_ops.pending(dev, 0)) {
			while (ops->pending(dev, 0)) {
				if (++i == 0x1000000) break;
			}
		}
		gpio_set_value(G1_TX_ENABLE, 0);
		gpio_set_value(G1_RX_ENABLE, 1);

	} else if (op == 1) {
		//plat->tx_active = 1;
		gpio_set_value(G1_RX_ENABLE, 0);
		gpio_set_value(G1_TX_ENABLE, 1);

	} else if (op == 2) {
		udelay(1000);
	}
}

#endif

/* int board_early_init_r(void) */
int rk_board_late_init(void)
{
	__maybe_unused struct unipi_handoff *handoff;

#if CONFIG_IS_ENABLED(BLOBLIST) && !defined(CONFIG_SPL_BUILD)
	handoff = bloblist_find(BLOBLISTT_UNIPI_DDR, sizeof(struct unipi_handoff));
	if ((handoff != NULL) && (handoff->ddr<=1)) {
		char num[10];
		sprintf(num, "%u", handoff->ddr);
		env_set("unipi_dram_type", num);
		if (handoff->ddr != 0) {
			/* on old boards is not required compatibility flag */
			env_set("boot_a_script", "run boot_c_script");
		}
	} else {
		/* on error  is REQUIRED compatibility flag */
		env_set("boot_a_script", "run boot_c_script");
	}
#endif
	return 0;
}

int mach_cpu_init(void)
{
#ifndef CONFIG_SPL_BUILD
	/*
		- watch_dog trigger first global reset
		- tsadc trigger first global reset
	*/
	writel(readl(CRU_GLB_CNT_TH) | (3<<14), CRU_GLB_CNT_TH);
#endif
	return 0;
}

int check_button_status(int button_type)
{
	gpio_request(G1_BUTTON, "altboot button");
	gpio_direction_input(G1_BUTTON);
	if (gpio_get_value(G1_BUTTON) == 0)
		return 0;

	gpio_request(G1_TX_ENABLE, "console tx enable");
	gpio_request(G1_RX_ENABLE, "console rx enable");
	gpio_direction_output(G1_TX_ENABLE,0);
	gpio_direction_output(G1_RX_ENABLE,1);
	//tx_active = 0;
	rs485_enable = 1;
	return 1;
}


int ft_board_setup(void *blob, struct bd_info  *bd)
{
	char tmpstr[20];
	ulong ddr;
	ddr = env_get_ulong("unipi_dram_type", 10, 0xffff);
	if (ddr == 0) {
		snprintf(tmpstr, sizeof(tmpstr)-1, "g1 1.0");
	} else {
		snprintf(tmpstr, sizeof(tmpstr)-1, "g1 2.0");
	}
	tmpstr[sizeof(tmpstr)-1]= '\0';
	fdt_setprop(blob, 0, "altboot-hwrevision", tmpstr, strlen(tmpstr));
	return ft_unipi_board_setup(blob, bd);
}

int identify_usb_hub(struct usb_hub_device *hub, int port)
{
	/* modify delay only for root hub - on G1 there is only one usb controller/root hub */
	if (hub && (hub->hub_depth == -1) && (port==1))
		return 1;
	return 0;
}
