# Build the ARM Trusted firmware for Zulu 1GB with OP-TEE
-------------------------------------------------------
RAM size affects the address of optee location.
 - The address is 0x7e000000 for 1GB
 - The address is 0xbe000000 for 2GB

This settings into ATF, OP-TEE and U-boot.
```
   $ cd atf
   $ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make -j $(nproc) \
          PLAT=imx8mm \
          IMX_BOOT_UART_BASE=0x30880000 \
          BL32_BASE=0x7e000000 \
          SPD=opteed \
          bl31
```

Build the OP-TEE binary for Zulu 1GB DRAM
-----------------------------------------
Download source repo
```
   git clone https://github.com/OP-TEE/optee_os.git
   cd optee_os
```
or use NXP repo
```
   git clone https://github.com/nxp-imx/imx-optee-os.git
   cd imx-optee-os
```
Compile and install tee.bin. Platform must be chosen according to RAM size
```
   CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm make -j $(nproc) \
             O=out/arm \
             PLATFORM=imx-mx8mm_phyboard_polis

   cp out/arm/core/tee-raw.bin ../u-boot/tee.bin
```

Built U-boot with OP-TEE
------------------------
Set config variables:
- CONFIG_TEE=y
- CONFIG_OPTEE=y
- CONFIG_IMX8M_OPTEE_LOAD_ADDR=0x7e000000

Build U-boot image.
