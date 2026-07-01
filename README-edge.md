# Build U-boot for Unipi Edge

Prepare the build environment and compile the bootloader as described in
[README.md](README.md), using `DEFCONFIG=unipi-edge_defconfig`.


# Install U-boot image to Edge

The bootloader image must be installed into the Edge MMC boot partition (FAT),
usually mounted as `/boot/firmware`. U-Boot is not the primary bootloader; it
is loaded by the Raspberry Pi firmware. The firmware must be configured in
`config.txt` to load `u-boot.bin`.

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
