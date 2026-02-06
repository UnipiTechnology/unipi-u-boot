#!/usr/bin/make -f

#DEFCONFIG = "unipi-edge_defconfig"
#DEFCONFIG = "unipi-zulu-plc_defconfig"
DEFCONFIG = "unipi-zulu_defconfig"

default: all

clean:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot clean
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C atf clean

configure:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot $(DEFCONFIG)

menuconfig:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot menuconfig

all:
	$(MAKE) -C atf PLAT=imx8mm -j 4 IMX_BOOT_UART_BASE=0x30880000 bl31
	@test -r u-boot/lpddr4_pmu_train_1d_dmem.bin
	@cd u-boot && rm -f bl31.bin; ln -s ../atf/build/imx8mm/release/bl31.bin bl31.bin; test -r bl31.bin
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot
