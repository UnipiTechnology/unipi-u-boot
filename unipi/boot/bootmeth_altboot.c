// SPDX-License-Identifier: GPL-2.0
/*
 * Bootmethod for booting via a U-Boot script loaded from /boot/altboot
 *
 * Copyright (C) 2024 Miroslav Ondra
 * Author: Miroslav Ondra <ondra@unipi.technology>
 *
 * Based on bootmeth_script.c driver written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <blk.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <env.h>
#include <fs.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <net.h>

/* These are used if filename-prefixes */
const char * alt_prefixes[] = {"/altboot/", "/boot/altboot/", NULL};

static int altboot_check(struct udevice *dev, struct bootflow_iter *iter);

static int altboot_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct bootstd_priv * std_priv;
	const char **old_prefixes;
	int ret;
	struct udevice *sdev;
	const struct bootmeth_ops *sops;

	ret = bootstd_get_priv(&std_priv);
	if (ret)
		return log_msg_ret("std", ret);
	old_prefixes = std_priv->prefixes;
	std_priv->prefixes = alt_prefixes;

	ret = uclass_get_device_by_name(UCLASS_BOOTMETH, "script", &sdev);
	if (ret) {
		return log_msg_ret("script", -ENOTSUPP);
	}
	sops = bootmeth_get_ops(sdev);
	ret = sops->read_bootflow(dev, bflow);
	std_priv->prefixes = old_prefixes;
	if (ret)
		return log_msg_ret("blk", ret);

	return 0;
}


static int altboot_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = IS_ENABLED(CONFIG_BOOTSTD_FULL) ?
		"Alternate boot from a block device" : "altboot";

	return 0;
}

static struct bootmeth_ops altboot_bootmeth_ops = {
	.check		= altboot_check,
	.read_bootflow	= altboot_read_bootflow
};

static int altboot_check(struct udevice *dev, struct bootflow_iter *iter)
{
	//struct bootmeth_ops *ops = bootmeth_get_ops(dev);
	struct udevice *sdev;
	const struct bootmeth_ops *sops;
	int ret;
	/* This works on block devices, network devices and SPI Flash */
	if (iter->method_flags & BOOTFLOW_METHF_PXE_ONLY)
		return log_msg_ret("pxe", -ENOTSUPP);

	ret = uclass_get_device_by_name(UCLASS_BOOTMETH, "script", &sdev);
	if (ret) {
		return log_msg_ret("altboot", -ENOTSUPP);
	}
	sops = bootmeth_get_ops(sdev);
	altboot_bootmeth_ops.set_bootflow = sops->set_bootflow;
	altboot_bootmeth_ops.read_file = sops->read_file;
	altboot_bootmeth_ops.boot = sops->boot;
	return 0;
}



static const struct udevice_id altboot_bootmeth_ids[] = {
	{ .compatible = "u-boot,altboot" },
	{ }
};

/* Put an number before 'altboot' to provide a default ordering */
U_BOOT_DRIVER(bootmeth_5altboot) = {
	.name		= "bootmeth_altboot",
	.id		= UCLASS_BOOTMETH,
	.of_match	= altboot_bootmeth_ids,
	.ops		= &altboot_bootmeth_ops,
	.bind		= altboot_bootmeth_bind,
};
