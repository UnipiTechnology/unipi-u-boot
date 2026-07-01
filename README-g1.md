# Build U-boot for Unipi G1

Prepare the build environment and compile the bootloader as described in
[README.md](README.md), using `DEFCONFIG=unipi-g1_defconfig`.


# Install U-boot image to G1

The bootloader can be installed either to the internal mmc
(done by using the Debian packages on a running system)
or to an external SD-card for development purposes.

After a successful build, the SD-card can be flashed with `dd`.

> Always double-check the **of** parameter.

```bash
dd bs=1k seek=32 if=$UBOOT_PATH/u-boot/idbloader.img of=/dev/sdX
dd bs=1k seek=8192 if=$UBOOT_PATH/u-boot/u-boot.itb of=/dev/sdX
```

