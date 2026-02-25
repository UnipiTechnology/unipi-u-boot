/*
 *
 * Copyright (c) 2025  Unipi Technology, ondra@unipi.technology
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef __UNIPI_SYSTEM_H__
#define __UNIPI_SYSTEM_H__

#include <usb.h>

/* This must be defined before loading uniee.h */
#define htobe16(x) cpu_to_be16(x)
#include "uniee.h"

int ft_unipi_board_setup(void *blob, struct bd_info *bd);
int check_button_status(int button_type);
int identify_usb_hub(struct usb_hub_device *hub, int port);

#endif /* __UNIPI_SYSTEM_H__*/
