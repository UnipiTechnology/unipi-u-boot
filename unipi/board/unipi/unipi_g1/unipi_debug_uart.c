// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2021 Miroslav Ondra, Faster CZ (Unipi Technology)
 */

#include <init.h>
#include <spl_gpio.h>
#include <asm/arch-rockchip/bootrom.h>
#include <asm/arch-rockchip/hardware.h>
#include <asm/arch-rockchip/grf_rk3328.h>
#include <asm/arch-rockchip/uart.h>
#include <asm/arch-rockchip/gpio.h>
#include <asm/armv8/mmu.h>
#include <asm/io.h>


#define CRU_BASE		0xFF440000
#define GRF_BASE		0xFF100000
#define UART2_BASE		0xFF130000

#define GPIO2_BASE		0xFF230000


#ifdef CONFIG_SPL_BUILD

static void tpl_gpio_set_input(const struct rockchip_gpio_regs * gpioreg, int gpio)
{
	clrsetbits_le32(&gpioreg->swport_ddr, 1 << (gpio), GPIO_INPUT << (gpio));
}

static int tpl_gpio_read(const struct rockchip_gpio_regs * gpioreg, int gpio)
{
	return (readl(&gpioreg->ext_port) & (1 << gpio)) != 0;
}


int tpl_gpio_set_pull(void *vregs, uint gpio, int pull)
{
	u32 *regs = vregs;
	uint val;

	regs += gpio >> GPIO_BANK_SHIFT;
	gpio &= GPIO_OFFSET_MASK;
	switch (pull) {
	case GPIO_PULL_UP:
		val = 1;
		break;
	case GPIO_PULL_DOWN:
		val = 2;
		break;
	default:
		val = 0;
		break;
	}

	rk_clrsetreg(regs, 3 << (gpio * 2), val << (gpio * 2));

	return 0;
}

int tpl_gpio_output(void *vregs, uint gpio, int value)
{
	struct rockchip_gpio_regs * const regs = vregs;

	clrsetbits_le32(&regs->swport_dr, 1 << gpio, value << gpio);

	/* Set direction */
	clrsetbits_le32(&regs->swport_ddr, 1 << gpio, 1 << gpio);

	return 0;
}

#endif /* CONFIG_SPL_BUILD */

void board_debug_uart_init(void)
{
	struct rk3328_grf_regs * const grf = (void *)GRF_BASE;
	struct rk_uart * const uart = (void *)UART2_BASE;
	enum{
		GPIO2A0_SEL_SHIFT       = 0,
		GPIO2A0_SEL_MASK        = 3 << GPIO2A0_SEL_SHIFT,
		GPIO2A0_UART2_TX_M1     = 1,

		GPIO2A1_SEL_SHIFT       = 2,
		GPIO2A1_SEL_MASK        = 3 << GPIO2A1_SEL_SHIFT,
		GPIO2A1_UART2_RX_M1     = 1,

		GPIO2C2_SEL_SHIFT       = 6,
		GPIO2C2_SEL_MASK        = 7 << GPIO2C2_SEL_SHIFT,
		GPIO2C2_GPIO            = 0,

		GPIO2C5_SEL_SHIFT       = 0,
		GPIO2C5_SEL_MASK        = 7 << GPIO2C5_SEL_SHIFT,
		GPIO2C5_GPIO            = 0,
	};
	enum {
		IOMUX_SEL_UART2_SHIFT   = 0,
		IOMUX_SEL_UART2_MASK    = 3 << IOMUX_SEL_UART2_SHIFT,
		IOMUX_SEL_UART2_M0      = 0,
		IOMUX_SEL_UART2_M1,
	};

#ifdef CONFIG_SPL_BUILD
	struct rockchip_gpio_regs * const gpio2 = (void *)GPIO2_BASE;

	/* set GPIO2-C2 to gpio mode, output value 0 (RTS off)*/
	rk_clrsetreg(&grf->gpio2cl_iomux,
		     GPIO2C2_SEL_MASK,
		     GPIO2C2_GPIO << GPIO2C2_SEL_SHIFT);
	tpl_gpio_output(gpio2, GPIO(BANK_C, 2), 0); 

	/* set GPIO2-C5 to gpio-mode, direction to input mode with pulldown - switch default value is 0 */
	rk_clrsetreg(&grf->gpio2ch_iomux,
		     GPIO2C5_SEL_MASK,
		     GPIO2C5_GPIO << GPIO2C5_SEL_SHIFT);
	tpl_gpio_set_input(gpio2, GPIO(BANK_C, 5));
	tpl_gpio_set_pull(&grf->gpio2a_p, GPIO(BANK_C, 5), GPIO_PULL_DOWN);
	//rk_clrsetreg(&grf->gpio2c_p , GPIO5_PULL_MASK, GPIO5_PULLDOWN);
#endif

	/* uart_sel_clk default select 24MHz */
	writel((3 << (8 + 16)) | (2 << 8), CRU_BASE + 0x148);

	/* init uart baud rate 115200 */
	writel(0x83, &uart->lcr);
	writel(208, &uart->rbr);
	writel(0x3, &uart->lcr);

	/* Enable early UART2 */
	rk_clrsetreg(&grf->com_iomux,
		     IOMUX_SEL_UART2_MASK,
		     IOMUX_SEL_UART2_M1 << IOMUX_SEL_UART2_SHIFT);
	rk_clrsetreg(&grf->gpio2a_iomux,
		     GPIO2A0_SEL_MASK,
		     GPIO2A0_UART2_TX_M1 << GPIO2A0_SEL_SHIFT);
	rk_clrsetreg(&grf->gpio2a_iomux,
		     GPIO2A1_SEL_MASK,
		     GPIO2A1_UART2_RX_M1 << GPIO2A1_SEL_SHIFT);

	/* enable FIFO */
	writel(0x1, &uart->sfe);


#ifdef CONFIG_SPL_BUILD
	/* check switch */
	tpl_gpio_read(gpio2, GPIO(BANK_C, 5));
	if (tpl_gpio_read(gpio2, GPIO(BANK_C, 5))) {
		tpl_gpio_output(gpio2, GPIO(BANK_C, 2), 1);
	}
#endif
}
