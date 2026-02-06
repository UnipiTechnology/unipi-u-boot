# Build U-boot for Unipi Zulu

This text describes cross-build of U-boot on amd64 Linux host.

Required packages - package names are valid for Debian 13
 - build-essential
 - git
 - bison
 - flex
 - python3
 - python3-setuptools
 - swig
 - cmake
 - python3-dev
 - libssl-dev
 - libncurses-dev
 - crossbuild-essential-arm64
 - wget

## Prepare building environment
Load source repo of unipi-u-boot and download all required sources
```git clone https://git.unipi.technology:unipi/unipi-u-boot.git
   cd unipi-u-boot
   ./prepare.sh
```
Script prepare.sh downloads sources of u-boot, atf, firmware. During extracting firmware,
you must to accept license of DDR firmware. After extracting sources patches are applied to
sources.

If source code development is not expected, call prepare.sh with the _build_ parameter.
Local git repositories will not be created.
```
   ./prepare.sh build
```

## Compile binary
Use prepared configuration file for Unipi Zulu.
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make configure DEFCONFIG=unipi-zulu_defconfig
```
Debug console of U-boot is set to UART3.

Bootloader supports loading bootscripts boot.scr or extlinux from mmc, usb flash, tftp.

Signing of U-boot binary is disabled, because it requires to prepare signing tool and keys.

To modifying U-boot building options use menuconfig, but be very careful!
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make menuconfig
```

Build bootloader image
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make
```
To speedup build append parameter -j <number of cores>
The first step of build process create ATF (ARM Truested Firmware), then U-boot and SPL. All that
parts and DDR firmware are packed into one image in directory u-boot name **flash.bin**


# Install U-boot image to Zulu

Final image flash.bin must be transfered to Zulu's EMMC to sector 66 (33kB from start of block device).

> !! This operation is very DANGEROUS !!

If you inappropriately change any important parameter during the build, the next boot may get stuck.
Repairing your Zulu without a hardware tool might be impossible.

For example using dd on Zulu
```
    sudo dd bs=1k seek=33 if=flash.bin of=/dev/mmcblk2 conv=sync
```

