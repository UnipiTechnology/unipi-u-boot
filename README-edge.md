# Build U-boot for Unipi Edge

Prepare building environment as described in [README.md](README.md)


## Configure and compile bootloader binary

Use prepared configuration file for Unipi Zulu.
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make configure DEFCONFIG=unipi-edge_defconfig
```
Debug console of U-boot is set to ttyAMA1.

Bootloader supports loading bootscripts boot.scr or extlinux from mmc, usb flash, tftp.

To modifying U-boot building options use menuconfig, but be very careful!
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make menuconfig
```

Build bootloader image
```
  CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make
```
To speedup build append parameter -j $(nproc)

Build process creates bootloader image u-boot/u-boot.bin


# Install U-boot image to Edge

Bootloader image must be installed into Edge MMC boot partition (FAT) usually mounted as
/boot/firmware. U-boot is not the primary bootloader, it is loaded by Raspberry Pi firmware.
The firmware must be configured in config.txt to load u-boot.bin.

Example of usable config.txt file:
```
dtoverlay=unipi_uboot_tpm

# required parameters!!!
arm_64bit=1
enable_uart=1
ignore_lcd=1
kernel=u-boot.bin

# optional param
#uart_2ndstage=1

[cm4]
# required to enable usb on compute module 4
otg_mode=1
[all]

```
