/*
 *
 * Copyright (c) 2021  Faster CZ, ondra@faster.cz
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <asm/io.h>
#include <usb.h>

#define htobe16(x) cpu_to_be16(x)

#include "uniee.h"
#include "uniee_values.h"
#include "unipi_eprom.h"

static void slotstr(u8* slot, int size, char* dest)
{
	int i;
	char ch;
	for (i=0; i<size; i++) {
		dest[i] = ch = slot[i];
		if (ch == 0xff || ch == '\0') {
			dest[i] = '\0';
			break;
		}
	}
}


/**
  * Find property in memory block from Unipi Eprom
  * @returns:
  *   - pointer to data and length in len
  *   - NULL if property not found or any error in structure
*/
#define specdata_count 12
#define specdata_size  (2*UNIEE_BANK_SIZE)

u8* unipi_eeprom_find_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, int* len)
{
	int cur_type, i;
	int dataindex = 0;

	for (i=0; i < specdata_count; i++) {
		cur_type = descriptor->board_info.specdata_headers_table[i].field_type |
                   (((int)(descriptor->board_info.specdata_headers_table[i].field_len & (~0x3f)))<<2);
		*len = descriptor->board_info.specdata_headers_table[i].field_len & (0x3f);
		if ((dataindex + *len) >= specdata_size)
			return NULL;
		if (cur_type == property_type) {
			return eprom + dataindex;
		}
		dataindex += *len;
	}
	return NULL;
}

/**
  * Find unsigned integer property in memory block from Unipi Eprom
  * @returns:
  *   - original length of property data (0-8) and uint value
  *   - negative number if error
*/
int unipi_eeprom_get_uint_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, unsigned long *value)
{
	int len;
	u8* ptr = unipi_eeprom_find_property(eprom, descriptor, property_type, &len);
	if (ptr == NULL) return -1;
	if (len > sizeof(*value)) return -1;
	*value = 0;
	memcpy(value, ptr, len);
	return len;
}

/**
  * Find bytes property in memory block from Unipi Eprom
  * @returns:
  *   - original length of property data
  *   - negative number if error
*/
int unipi_eeprom_get_bytes_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, u8* bytes, int maxlen)
{
	int len;
	u8 *ptr = unipi_eeprom_find_property(eprom, descriptor, property_type, &len);

	if (len <= 0) return len;
	memcpy(bytes, ptr, min(len,maxlen));
	return len;
}
/**
  * Find string property in memory block from Unipi Eprom
  * @returns:
  *   - original length of property data and null terminated str
  *   - negative number if error
*/
int unipi_eeprom_get_str_property(u8 *eprom, uniee_descriptor_area* descriptor, int property_type, char* str, int maxlen)
{
	int len;
	u8 *ptr = unipi_eeprom_find_property(eprom, descriptor, property_type, &len);

	if (len < 0) return len;
	if (len == 0) {
		str[0] = '\0';
	} else {
		slotstr(ptr, min(len,maxlen-1), str);
		str[min(len,maxlen-1)] = '\0';
	}
	return len;
}


u32 unipi_eeprom_get_serial(uniee_descriptor_area* descriptor)
{
	u32 serial = descriptor->product_info.product_serial;
	if (serial == 0xffffffff) serial = 0;
	return serial;
}

u32 unipi_eeprom_get_sku(uniee_descriptor_area* descriptor)
{
	u32 sku = descriptor->product_info.sku;
	if (sku == 0xffffffff) sku = 0;
	return sku;
}

void unipi_eeprom_get_model(uniee_descriptor_area* descriptor, char* str, int maxlen)
{
	int len = min((int) sizeof(descriptor->product_info.model_str), maxlen-1);
	slotstr(descriptor->product_info.model_str, len, str);
	str[len]= '\0';
}
