// SPDX-License-Identifier: GPL-2.0
/*
 * Bootmethod for one-shot boot from other location
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
#include <bootcount.h>
#include <command.h>

#define TRYBOOT_FNAME1	"tryboot"
#define TRYBOOT_FNAME2	"recover.scr"

static int tryboot_check(struct udevice *dev, struct bootflow_iter *iter)
{
	const struct udevice *media = dev_get_parent(iter->dev);
	enum uclass_id id = device_get_uclass_id(media);
	unsigned long bootcount = env_get_ulong("bootcount", 10, 0);

	/* This only works on mmc devices */
	if ((id != UCLASS_MMC) || (bootcount == 0))
		return -ENOTSUPP;
	return 0;
}

static int tryboot_read_bootflow(struct udevice *bootstd,
				     struct bootflow *bflow)
{
	struct blk_desc *desc = NULL;
	const char *const *prefixes;
	const char *prefix;
	int ret, i;

	ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
	if (ret)
		return log_msg_ret("std", ret);

	if (bflow->blk) {
		/* We require a partition table */
		if (!bflow->part)
			return -ENOENT;
		 desc = dev_get_uclass_plat(bflow->blk);
	}

	prefixes = bootstd_get_prefixes(bootstd);
	i = 0;
	do {
		prefix = prefixes ? prefixes[i] : NULL;

		ret = bootmeth_try_file(bflow, desc, prefix, TRYBOOT_FNAME1);
		if (ret) {
			ret = bootmeth_try_file(bflow, desc, prefix,
						TRYBOOT_FNAME2);
		} else {
			bflow->os_name = strdup("tryboot");
		}
	} while (ret && prefixes && prefixes[++i]);
	if (ret)
		return log_msg_ret("try", ret);

	bflow->subdir = strdup(prefix ? prefix : "");
	if (!bflow->subdir)
		return log_msg_ret("prefix", -ENOMEM);

	ret = bootmeth_alloc_file(bflow, 0x10000, ARCH_DMA_MINALIGN,
	                          (enum bootflow_img_t)IH_TYPE_SCRIPT);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

static int tryboot_set_bootflow(struct udevice *dev, struct bootflow *bflow,
			       char *buf, int size)
{
	buf[size] = '\0';
	bflow->buf = buf;
	bflow->size = size;
	bflow->state = BOOTFLOWST_READY;
	return 0;
}


static int tryboot_boot(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootmeth_ops *sops;
	struct udevice *sdev;
	int ret = 0;
#if IS_ENABLED(CONFIG_BOOTCOUNT_LIMIT)
	bootcount_store(0);
	env_set("bootcount", "0");
#endif
	if (bflow->os_name && (strcmp(bflow->os_name, "tryboot")==0)) {
		for(char* s=bflow->buf; *s != '\0'; ++s) {
			if ((*s == '\n') || (*s == '\r')) {
				*s = '\0';
				break;
			}
		}
		printf("TRYBOOT: %s\n", bflow->buf);
		run_commandf("bootflow scan -b %s", bflow->buf);
		do_reset(NULL, 0, 0, NULL);
		return -1;
	} else {
		ret = uclass_get_device_by_name(UCLASS_BOOTMETH, "script", &sdev);
		if (ret) {
			return log_msg_ret("tryboot", -ENOTSUPP);
		}
		sops = bootmeth_get_ops(sdev);
		return sops->boot(dev, bflow);
	}
}

static int tryboot_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = IS_ENABLED(CONFIG_BOOTSTD_FULL) ?
		"Try boot from other partition" : "tryboot";

	return 0;
}

static struct bootmeth_ops tryboot_bootmeth_ops = {
	.check		= tryboot_check,
	.read_bootflow	= tryboot_read_bootflow,
	.set_bootflow	= tryboot_set_bootflow,
	.read_file	= bootmeth_common_read_file,
	.boot		= tryboot_boot,
};

static const struct udevice_id tryboot_bootmeth_ids[] = {
	{ .compatible = "u-boot,tryboot" },
	{ }
};

U_BOOT_DRIVER(bootmeth_tryboot) = {
	.name		= "bootmeth_tryboot",
	.id		= UCLASS_BOOTMETH,
	.of_match	= tryboot_bootmeth_ids,
	.ops		= &tryboot_bootmeth_ops,
	.bind		= tryboot_bootmeth_bind,
};
