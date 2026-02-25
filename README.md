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

For further instructions, see the device-specific file.

 - [Unipi Zulu](README-zulu.md)
 - Unipi Patron
 - [Unipi Edge](README-edge.md)
 - Unipi G1
