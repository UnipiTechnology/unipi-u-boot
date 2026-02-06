// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */
/******************************************************************
 *
 *  THIS FILE IS GENERATED FROM TEMPLATE. DON'T MODIFY IT
 *
 * uniee_values.h
 *
 *  Created on: Jan 14, 2022
 *      Author: Miroslav Ondra <ondra@faster.cz>
 *
 */

#ifndef UNIEE_VALUES_H_
#define UNIEE_VALUES_H_

/* uniee_bank_3_t.platform_id */
 #define UNIEE_PLATFORM_FAMILY_UNIPI1 0x01
 #define UNIEE_PLATFORM_FAMILY_G1XX   0x02
 #define UNIEE_PLATFORM_FAMILY_NEURON 0x03
 #define UNIEE_PLATFORM_FAMILY_AXON   0x05
 #define UNIEE_PLATFORM_FAMILY_EDGE   0x06
 #define UNIEE_PLATFORM_FAMILY_PATRON 0x07
 #define UNIEE_PLATFORM_FAMILY_IRIS   0x0f
 #define UNIEE_PLATFORM_FAMILY_OEM    0x10

/* specdata_header_t.field_type */
 #define UNIEE_FIELD_TYPE_BUTTON 4
 #define UNIEE_FIELD_TYPE_RTC    5
 #define UNIEE_FIELD_TYPE_MAC    6
 #define UNIEE_FIELD_TYPE_AICAL  7
 #define UNIEE_FIELD_TYPE_MAC1   8
 #define UNIEE_FIELD_TYPE_MODEM  9


/* values of fields stored in uniee_bank_1_t.dummy_data */

 #define UNIEE_FIELD_VALUE_BUTTON_PATRON 0
 #define UNIEE_FIELD_VALUE_BUTTON_IRIS   1
 #define UNIEE_FIELD_VALUE_BUTTON_NEURON 2
 #define UNIEE_FIELD_VALUE_BUTTON_EDGE   3

struct uniee_map {
  int index;
  const char *name;
};

/* specdata_header_t.field_type */
#define UNIEE_FIELD_TYPE_MAP {\
 { 4,"BUTTON" },\
 { 5,"RTC" },\
 { 6,"MAC" },\
 { 7,"AICAL" },\
 { 8,"MAC1" },\
 { 9,"MODEM" },\
}

#define DIM(x) (sizeof(x)/sizeof(*(x)))

#endif