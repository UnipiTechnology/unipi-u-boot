/*
 *
 * Copyright (c) 2025  Unipi Technology, ondra@unipi.technology
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <dm/uclass.h>
#include <env.h>
#include <fdt_support.h>
#include <i2c_eeprom.h>
#include <net-common.h>
#include <rtc.h>
#include <stdio.h>
#include <usb.h>

/* This must be defined before loading uniee.h */
#define htobe16(x) cpu_to_be16(x)

#include "uniee.h"
#include "uniee_values.h"
#include "unipi_eprom.h"

int mac_read_from_eeprom(void);
void usb_hub_reset_devices(struct usb_hub_device *hub, int port);
int ft_unipi_board_setup(void *blob, struct bd_info *bd);

/* define weak functions - can be redifned in board file */
__weak int check_button_status(int button_type) {return 0;}
__weak int identify_usb_hub(struct usb_hub_device *hub, int port) {return 0;}

#if IS_ENABLED(CONFIG_ID_EEPROM)

#define UNIPI_EEPROM_SIZE UNIEE_MIN_EE_SIZE
static u8 unipi_eprom[UNIPI_EEPROM_SIZE] __aligned(32);
static int unipi_eprom_validity = -1;
uniee_descriptor_area* uniee_descriptor;

static int get_unipi_eeprom(void)
{
	int ret;
	struct udevice *dev;

	if (unipi_eprom_validity >= 0)
		return unipi_eprom_validity;

	ret = uclass_first_device_err(UCLASS_I2C_EEPROM, &dev);
	if (ret)
		return ret;

	ret = i2c_eeprom_read(dev, 0x0, unipi_eprom, UNIPI_EEPROM_SIZE);
	if (ret)
		return ret;

	/* check unipi mark */
	uniee_descriptor = uniee_get_valid_descriptor(unipi_eprom, UNIPI_EEPROM_SIZE);
	if (uniee_descriptor == NULL) {
		unipi_eprom_validity = 1;
		return -1;
	}

	uniee_fix_legacy_content(unipi_eprom, UNIPI_EEPROM_SIZE, uniee_descriptor);
	unipi_eprom_validity = 0;
	return 0;
}

static void load_mac_address(u8 index)
{
	u8 ethaddr[6];
	if (get_unipi_eeprom() != 0)
		return;

	if (unipi_eeprom_get_bytes_property(unipi_eprom, uniee_descriptor,
	    index?UNIEE_FIELD_TYPE_MAC1:UNIEE_FIELD_TYPE_MAC,
	    ethaddr, 6) == 6) {
		if (is_valid_ethaddr(ethaddr)) {
			if (index == 0)
				env_set("ethaddr", NULL);
			eth_env_set_enetaddr_by_index("eth", index,ethaddr);
		}
	}
}

#define MCP794XX_REG_CALIBRATION        0x08
static void load_rtc_calibration(void)
{
	char calibration;
	struct udevice *dev;
	int ret;

	if (get_unipi_eeprom() != 0)
		return;
	if (unipi_eeprom_get_bytes_property(unipi_eprom, uniee_descriptor, UNIEE_FIELD_TYPE_RTC, &calibration, 1) != 1)
		return;
	ret = uclass_get_device_by_name(UCLASS_RTC, "mcp7940x@6f", &dev);
	if (ret != 0) {
		ret = uclass_get_device_by_name(UCLASS_RTC, "rtc@6f", &dev);
		if (ret != 0)
			return;
	}
	rtc_write8(dev, MCP794XX_REG_CALIBRATION, calibration);
}

static void read_button(void)
{
	/* old Patrons without defined button */
	ulong button = UNIEE_FIELD_VALUE_BUTTON_PATRON;
	if (unipi_eeprom_get_uint_property(unipi_eprom, uniee_descriptor, UNIEE_FIELD_TYPE_BUTTON, &button) >= 0) {
		/* button is defined, disable bootdelay, can be overriden in check_button_status */
		env_set("bootdelay", "-2");
	}
	if (check_button_status(button)) {
		printf("Switch off BUTTON and press a KEY on console to break autoboot!\n");
		env_set("bootdelay", "2");
		env_set("bootcmd", "run boot_altboot;bootflow scan -lb");
	}
}

int mac_read_from_eeprom(void)
{
	if (get_unipi_eeprom() != 0)
		return 0;

	load_mac_address(0);
	load_mac_address(1);
	load_rtc_calibration();

	read_button();
	return 0;
}

int ft_unipi_board_setup(void *blob, struct bd_info *bd)
{
	char tmpstr[20];
	if (get_unipi_eeprom() != 0)
		return 0;

	if (snprintf(tmpstr, sizeof(tmpstr)-1, "%u", unipi_eeprom_get_serial(uniee_descriptor)) >= 0) {
		tmpstr[sizeof(tmpstr)-1]= '\0';
		fdt_setprop(blob, 0, "unipi-serial", tmpstr, strlen(tmpstr) + 1);
	}
	if (snprintf(tmpstr, sizeof(tmpstr)-1, "%u", unipi_eeprom_get_sku(uniee_descriptor)) >= 0) {
		tmpstr[sizeof(tmpstr)-1]= '\0';
		fdt_setprop(blob, 0, "unipi-sku", tmpstr, strlen(tmpstr) + 1);
	}
	unipi_eeprom_get_model(uniee_descriptor, tmpstr, sizeof(tmpstr));
	fdt_setprop(blob, 0, "unipi-model", tmpstr, strlen(tmpstr) + 1);
	return 0;
}

#else
int mac_read_from_eeprom(void)
{ 
	return 0;
}

int ft_unipi_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}
#endif


/* Add 2.5 s waiting during USB hub scan for particular interface */
void usb_hub_reset_devices(struct usb_hub_device *hub, int port)
{
	const char* env;
	unsigned int delay = 2500;

	if (identify_usb_hub(hub, port)) {
		env = env_get("usb_hub1_delay");
		if (env)
			delay = simple_strtoul(env, NULL, 0);
		printf("(Add usb_hub1_delay=%d ms) ", delay);
		hub->query_delay += delay;
		hub->connect_timeout += delay;
	}
}
