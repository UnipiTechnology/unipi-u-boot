// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */

#include <command.h>
#include <cpu_func.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx-regs.h>
#include <asm/sections.h>

#include <dm/uclass.h>
#include <dm/device.h>
//#include <dm/uclass-internal.h>
//#include <dm/device-internal.h>

#include <power/pmic.h>
#include <power/bd71837.h>

DECLARE_GLOBAL_DATA_PTR;

void lpddr_patch_timing(void);

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	switch (boot_dev_spl) {
	case SD2_BOOT:
	case MMC2_BOOT:
		return BOOT_DEVICE_MMC1;
	case SD3_BOOT:
	case MMC3_BOOT:
		return BOOT_DEVICE_MMC2;
	case USB_BOOT:
		return BOOT_DEVICE_BOARD;
	case SPI_NOR_BOOT:
		return BOOT_DEVICE_SPI;
	default:
		return BOOT_DEVICE_NONE;
	}
}

bool check_ram_available(long size)
{
	long sz = get_ram_size((long *)PHYS_SDRAM, size);

	if (sz == size)
		return true;

	return false;
}

struct fuse_bank14_regs {
	u32 id0;
	u32 rsvd0[3];
	u32 id1;
	u32 rsvd1[3];
	u32 serial;
	u32 rsvd2[3];
	u32 version;
	u32 rsvd3[3];
};

static int spl_read_unipi_otp(void)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank *bank = &ocotp->bank[14];
	struct fuse_bank14_regs *fuse =
		(struct fuse_bank14_regs *)bank->fuse_regs;

	u32 id0 = readl(&fuse->id0);
	u8 c1 = id0 & 0xff;
	u8 c3 = (id0 >> 16)&0xff;
	//u32 id1 = readl(&fuse->id1);
	u32 serial = readl(&fuse->serial);
	debug("Check OTP: %08x %c%c%c%c serial: %d\n", id0, c1, (id0 >> 8)&0xff, c3,\
                (id0 >> 24)&0xff, serial);
	if (c1 == 'Z') {
		if (c3 == '4')
			return 4;
		if (c3 == '2')
			return 2;
		if (c3 != '1')
			return 0;
		if ((serial == 0x1c9f)) {
			printf("Zulu 4GB pre-production sample\n");
			return 4;
		}
		return 1;
	} else if (((id0 >>8) & 0xff) == 'Z')
		return 1;
	return 0;
}

static void spl_dram_init(void)
{
	u32 size = 0;

	/*
	 * Try the default DDR settings in lpddr4_timing.c to
	 * comply with the Micron 4GB DDR.
	 */
	int otp_mem_size = spl_read_unipi_otp();
	debug("Memsize from OTP = %d GB\n", otp_mem_size);

	if ((otp_mem_size==0) && !ddr_init(&dram_timing) && check_ram_available(SZ_4G)) {
		size = 4;
	} else if ((otp_mem_size==4) && !ddr_init(&dram_timing)) {
		size = 4;
	} else {
		lpddr_patch_timing();
		if (!ddr_init(&dram_timing)) {
			if (check_ram_available(SZ_2G)) {
				size = 2;
			} else {
				size = 1;
			}
		}
	}
	printf("Unipi Zulu, %u GB RAM detected\n", size);
	writel(size, MCU_BOOTROM_BASE_ADDR);
}


int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pmic@4b", &dev);
	if (ret == -ENODEV) {
		puts("Failed to get PMIC\n");
		return 0;
	}
	if (ret != 0)
		return ret;

	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(dev, BD718XX_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x1);

	/* increase VDD_SOC to typical value 0.85v before first DRAM access */
	pmic_reg_write(dev, BD718XX_BUCK1_VOLT_RUN, 0x0f);

	/* increase VDD_DRAM to 0.975v for 3Ghz DDR */
	pmic_reg_write(dev, BD718XX_1ST_NODVS_BUCK_VOLT, 0x83);

#ifndef CONFIG_IMX8M_LPDDR4
	/* increase NVCC_DRAM_1V2 to 1.2v for DDR4 */
	pmic_reg_write(dev, BD718XX_BUCK8_VOLT, 0x28);
#endif

	/* lock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x11);

	return 0;
}

void spl_board_init(void)
{
	arch_misc_init();
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif

void board_init_f(ulong dummy)
{
	int ret;
//	struct udevice *dev;

	arch_cpu_init();

	init_uart_clk(2);

	timer_init();

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}

	preloader_console_init();

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
