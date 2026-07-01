# Build U-boot for Unipi Zulu

Prepare the build environment and compile the bootloader as described in
[README.md](README.md), using `DEFCONFIG=unipi-zulu_defconfig`.

Signing of the U-Boot binary is disabled, because it requires preparing the
signing tool and keys.


# Install U-boot image to Zulu

The final image `flash.bin` must be transferred to Zulu's eMMC to sector 66 (33 kB from the start of the block device).

> !! This operation is very DANGEROUS !!

If you inappropriately change any important parameter during the build, the next boot may get stuck.
Repairing your Zulu without a hardware tool might be impossible.

For example, using `dd` on Zulu:
```bash
sudo dd bs=1k seek=33 if=flash.bin of=/dev/mmcblk2 conv=sync
```

# Create boot script

Create a text file `boot.cmd` and build a `boot.scr`:
```bash
mkimage -C none -A arm -T script -d boot.cmd boot.scr
```
Copy the created file `boot.scr` to the `/boot` directory on the Zulu filesystem.

Sample of complex boot script is here:
```
kernel_mmc=${devnum}
kernel_part=${distro_bootpart}
setenv kernel vmlinux-6.12.66-cip16
setenv fdtfile unipi-zulu.dtb
setenv overlay unipi_s107
setenv loading_mmc ${devnum}:${distro_bootpart}
load mmc ${loading_mmc} ${kernel_addr_r} ${prefix}/${kernel}
if test "${ramdisk}" != ""; then
  load mmc ${loading_mmc} ${ramdisk_addr_r} ${prefix}/${ramdisk}
  setenv ramdisk_size 2000000
fi
load mmc ${loading_mmc} ${fdt_addr_r} ${prefix}/${fdtfile}
fdt addr ${fdt_addr_r}
fdt resize
if test "${overlay}" != "" && fdt list /__symbols__ resmem; then
    fdt resize 8192
    setexpr fdtovaddr ${fdt_addr_r} + C0000
    setenv break 0
    for ov in ${overlay}; do
      if test "${break}" = "0" ; then
        echo "Load and apply overlay ${ov}"
        if load mmc ${loading_mmc} ${fdtovaddr} ${prefix}/overlays/${ov}.dtb ; then
            if fdt apply ${fdtovaddr}; then
                echo "OK"
            else
                load mmc ${loading_mmc} ${fdt_addr_r} ${prefix}/${fdtfile}
                fdt addr ${fdt_addr_r}
                fdt resize
                setenv break 1
            fi
        fi
      fi
    done
fi
setenv fsck.repair yes
if test -e ${devtype} ${devnum}:${distro_bootpart} ${prefix}${aux_core_fw_name} ; then
  load ${devtype} ${devnum}:${distro_bootpart} ${aux_core_fw_addr_r} ${prefix}${aux_core_fw_name}
  run boot_aux_core
fi
fstype ${devtype} ${devnum}:${kernel_part} rootfstype
setenv rootdev /dev/mmcblk${kernel_mmc}p${kernel_part}
if test "${rootfstype}" = "ext4" ; then
  rootpar="rootfstype=${rootfstype} rw rootwait"
else
  rootpar="rw rootwait"
fi; fi
if test "${extra}" = "" ; then
   setenv extra panic=10 quiet
else 
   setenv extra console=${console},${baudrate} ${extra}
fi
setenv bootargs root=${rootdev} ${rootpar} systemd.gpt_auto=no fsck.repair=${fsck.repair} ${clock} ${extra}
if test "${ramdisk}" != ""; then
  booti ${kernel_addr_r} ${ramdisk_addr_r}:${ramdisk_size} ${fdt_addr_r}
else
  booti ${kernel_addr_r} - ${fdt_addr_r}
fi
```
