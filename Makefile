#!/usr/bin/make -f

#DEFCONFIG = "unipi-edge_defconfig"
#DEFCONFIG = "unipi-zulu-plc_defconfig"
DEFCONFIG = "unipi-zulu_defconfig"

ifeq ($(shell grep -q "^CONFIG_ROCKCHIP_RK3328=y" u-boot/.config && echo true),true)
  required += rk3328_bl31
endif
ifeq ($(shell grep -q "^CONFIG_ARCH_IMX8M=y" u-boot/.config && echo true),true)
  required += imx8mm_bl31
  required += u-boot/lpddr4_pmu_train_1d_dmem.bin
endif

default: all

clean:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot clean
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C atf clean

configure:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot $(DEFCONFIG)

menuconfig:
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot menuconfig


atf/build/rk3328/release/bl31/bl31.elf:
	$(MAKE) -C atf PLAT=rk3328 bl31

atf/build/imx8mm/release/bl31.bin:
	$(MAKE) -C atf PLAT=imx8mm IMX_BOOT_UART_BASE=0x30880000 bl31

rk3328_bl31: atf/build/rk3328/release/bl31/bl31.elf
	@cd u-boot && rm -f atf-bl31; ln -s ../atf/build/rk3328/release/bl31/bl31.elf atf-bl31
	@if ! [ -r u-boot/atf-bl31 ]; then echo "Missing ATF atf-bl31" >&2; false; fi

imx8mm_bl31: atf/build/imx8mm/release/bl31.bin
	@cd u-boot && rm -f bl31.bin; ln -s ../atf/build/imx8mm/release/bl31.bin bl31.bin
	@if ! [ -r u-boot/bl31.bin ]; then echo "Missing ATF bl31.bin" >&2; false; fi

all: $(required)
	MAKEFLAGS="$(MAKEFLAGS)" ARCH=arm $(MAKE) -C u-boot
	@if grep -q '^CONFIG_IMX_HAB=y' u-boot/.config; then \
	  [ -r u-boot/flash.bin ] && cp u-boot/flash.bin u-boot/signed-flash.bin;\
	fi
