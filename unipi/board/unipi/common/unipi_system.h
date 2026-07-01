/*
 *
 * Copyright (c) 2025  Unipi Technology, ondra@unipi.technology
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef __UNIPI_SYSTEM_H__
#define __UNIPI_SYSTEM_H__

/* This must be defined before loading uniee.h */
#define htobe16(x) cpu_to_be16(x)
#include "uniee.h"

int ft_unipi_board_setup(void *blob, struct bd_info *bd);
int check_button_status(int button_type);

#endif /* __UNIPI_SYSTEM_H__*/
