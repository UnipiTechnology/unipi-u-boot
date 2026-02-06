// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */

#include <config.h>
#include <dm.h>
#include <env.h>
#include <fdt_support.h>
#include <memalign.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/global_data.h>
#include <usb.h>
#include <linux/delay.h>

#include "../common/uniee_values.h"

DECLARE_GLOBAL_DATA_PTR;

#define EDGE_BUTTON   25
#define EDGE_RTS      17

int check_button_status(int button_type);
void rs485_tx_op(u32 *cr2, u32 *sr2, int op);
void rs485_bcmtx_op(u32 *cr2, u32 *sr2, int op);
void usb_hub_reset_devices(struct usb_hub_device *hub, int port);
int board_late_init(void);
int ft_system_setup(void *blob, struct bd_info  *bd);


int rs485_enable = 0;
int tx_active  = 0;

/* compatible with AMBA PL011 driver from serial_pl01x.c */
#define UART_PL011_CR_RTS               BIT(11)
#define UART_PL011_CR_RXE               BIT(9)
#define UART_PL01x_FR_TXFE              BIT(7)
#define UART_PL01x_FR_BUSY              BIT(3)
void rs485_tx_op(u32 *cr2, u32 *sr2, int op)
{
	if (rs485_enable != 1) return;
	if (op == 0) {
		if (tx_active) {
			int i = 0;
			while ((readl(sr2) & (UART_PL01x_FR_BUSY))) {
				if (++i == 0x1000000) break;
			}
			gpio_set_value(EDGE_RTS, 0);
			/* Enable RX, set alternatively UART driven RTS (inversed) */
			writel(readl(cr2) | UART_PL011_CR_RTS | UART_PL011_CR_RXE, cr2);
			tx_active = 0;
		}
	} else if (op == 1) {
		if (!tx_active) {
			/* Disable RX, clear alternatively UART driven RTS (inversed) */
			writel(readl(cr2) & (~(UART_PL011_CR_RXE | UART_PL011_CR_RTS)), cr2);
			gpio_set_value(EDGE_RTS, 1);
			tx_active = 1;
		}
	} else if (op == 2)
		udelay(1000);
}

/* compatible with BCM Mini UART  driver from serial_bcm283x_mu.c */
#define BCM283X_MU_LSR_TX_IDLE		BIT(6)
/* This actually means not full, but is named not empty in the docs */
#define BCM283X_MU_LSR_TX_EMPTY		BIT(5)
void rs485_bcmtx_op(u32 *cr2, u32 *sr2, int op)
{
	if (rs485_enable != 1) return;
	if (op == 0) {
		if (tx_active) {
			int i = 0;
			while (!(readl(sr2) & BCM283X_MU_LSR_TX_IDLE)) {
				if (++i == 0x1000000) break;
			}
			gpio_set_value(EDGE_RTS, 0);
			tx_active = 0;
		}
	} else if (op == 1) {
		if (!tx_active) {
			gpio_set_value(EDGE_RTS, 1);
			tx_active = 1;
		}
	} else if (op == 2)
		udelay(1000);
}

int check_button_status(int button_type)
{
	if (button_type != UNIEE_FIELD_VALUE_BUTTON_EDGE)
		return 0;
	//printf("BUTTON type:%ld value: %ld\n", button, value);
	gpio_request(EDGE_BUTTON, "edge button");
	gpio_direction_input(EDGE_BUTTON);
	if ( gpio_get_value(EDGE_BUTTON) == 0)
		return 0;

	gpio_request(EDGE_RTS, "edge rts");
	gpio_direction_output(EDGE_RTS, 0);
	tx_active = 0;
	rs485_enable = 1;
	return 1;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
    env_set("board_name", "UNIPI");
    env_set("board_rev", "EDGE");
#endif
    return 0;
}

/* check particular interface in USB hub - can connect usb storage*/
int identify_usb_hub(struct usb_hub_device *hub, int port)
{
	u32 reg;

	if (!hub || (hub->hub_depth != 0) || (port!=1))
		return 0;
	if (! dev_has_ofnode(hub->pusb_dev->controller_dev))
		return 0;
	if ( ofnode_read_u32(dev_ofnode(hub->pusb_dev->controller_dev), "reg", &reg))
		return 0;
	/* if (reg != 0x7e9c0000)
		return 0; */

	return 1; /* found */
}

int ft_system_setup(void *blob, struct bd_info  *bd)
{
    return ft_unipi_board_setup(blob,bd);
}
