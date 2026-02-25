/*
 *
 * Copyright (c) 2021  Faster CZ, ondra@faster.cz
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef __UNIPI_EPROM_H__
#define __UNIPI_EPROM_H__

#include "uniee.h"

int unipi_eeprom_get_uint_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, unsigned long *value);
int unipi_eeprom_get_bytes_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, u8* bytes, int maxlen);
u32 unipi_eeprom_get_serial(uniee_descriptor_area* descriptor);
u32 unipi_eeprom_get_sku(uniee_descriptor_area* descriptor);
void unipi_eeprom_get_model(uniee_descriptor_area* descriptor, char* str, int maxlen);

#endif /* __UNIPI_EPROM_H__*/
