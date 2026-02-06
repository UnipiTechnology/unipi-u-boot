# SPDX-License-Identifier: GPL-2.0
#
# (C) Copyright 2023 Unipi Technology s.r.o.
#

obj-y += unipi_zulu.o
obj-y += mmc.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_4g.o
endif


