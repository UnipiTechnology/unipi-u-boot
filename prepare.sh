#!/bin/bash

ATF_TAG=v2.12.0
UBOOT_TAG=v2026.01

DDR_TAG=8.28
IMX_DDR_ABBREV="994fa14"

create_git_repo()
{
    git init
    git add .
    git commit -m "Init from ${2}"
    git am $1/*
}

patch_source()
{
    for p in $1/*; do
        patch -p1 <$p
    done
}

if [ "$1" != "-na" ]; then
    wget "https://github.com/ARM-software/arm-trusted-firmware/archive/refs/tags/${ATF_TAG}.tar.gz" -O- | tar xz
    mv arm-trusted-firmware-* atf
    (
        cd atf
        if [ "$1" = "build" ]; then
            patch_source ../patches-a
        else
            create_git_repo ../patches-a "${ATF_TAG}"
        fi
    )
else
    shift
fi

if [ "$1" != "-nu" ]; then
    wget "https://source.denx.de/u-boot/u-boot/-/archive/${UBOOT_TAG}/u-boot-${UBOOT_TAG}.tar.gz" -O- | tar xz

    mv u-boot-${UBOOT_TAG} u-boot
    (
      cp -r unipi/* u-boot
      cd u-boot
      if [ "$1" = "build" ]; then
          patch_source ../patches-u
      else
          create_git_repo ../patches-u "${UBOOT_TAG}"
      fi
    )
else
    shift
fi

if [ "$1" != "-nd" ]; then
    DDR_FIRMWARE_NAME=" \
    lpddr4_pmu_train_1d_imem.bin \
    lpddr4_pmu_train_1d_dmem.bin \
    lpddr4_pmu_train_2d_imem.bin \
    lpddr4_pmu_train_2d_dmem.bin"

    FW="firmware-imx-$DDR_TAG-$IMX_DDR_ABBREV"
    wget https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/$FW.bin -O fwrun

    if [ "$1" = "build" ]; then 
        sh fwrun --auto-accept --force
    else
        sh fwrun --force
    fi
    rm -f fwrun
    mkdir -p "u-boot"
    # Synopsys DDR
    for ddr_firmware in ${DDR_FIRMWARE_NAME}; do
        install -m 0644 "$FW/firmware/ddr/synopsys/${ddr_firmware}" "u-boot"
    done
    rm -rf "$FW"
fi
