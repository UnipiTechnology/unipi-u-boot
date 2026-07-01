# U-Boot bootloader for Unipi PLCs

This repo contains recipes and patches to build U-boot bootloader
for Unipi PLCs and SBC
 
 - Unipi Zulu - mini SBC with NXP i.MX8M Mini
 - Unipi Patron - PLC with Unipi Zulu
 - Unipi Edge - PLC with Raspberry Pi CM40
 - Unipi G1 - PLC with Rockchip RK3328 (Config not finished yet!)


## Required tools for cross-build

List of required packages for build - package names are valid for Debian 13
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
```
   git clone https://git.unipi.technology:unipi/unipi-u-boot.git
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

## Build binary

### Configure and compile the bootloader

Use the prepared configuration file for your board:

```bash
CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make configure DEFCONFIG=<board>_defconfig
```

where `<board>_defconfig` is one of:

| Board | DEFCONFIG | Debug console | Output artifact(s) |
|---|---|---|---|
| Zulu / Patron | `unipi-zulu_defconfig` | UART3 | `u-boot/flash.bin` (ATF + U-Boot + SPL + DDR firmware) |
| Edge | `unipi-edge_defconfig` | ttyAMA1 | `u-boot/u-boot.bin` |
| G1 | `unipi-g1_defconfig` | — | `u-boot/u-boot.itb` + `u-boot/idbloader.img` |

The bootloader supports loading bootscripts `boot.scr` or `extlinux` from mmc, usb flash, tftp.

To modify U-Boot building options use `menuconfig`, but be very careful!

```bash
CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make menuconfig
```

Build the bootloader image:

```bash
CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make -j $(nproc)
```

The `-j $(nproc)` parameter speeds up the build by parallelizing it.

For installation instructions, see the device-specific file.

 - [Unipi Zulu](README-zulu.md)
 - Unipi Patron
 - [Unipi Edge](README-edge.md)
 - Unipi G1
