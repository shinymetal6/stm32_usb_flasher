/*
 * dfu-util
 *
 * Copyright 2007-2008 by OpenMoko, Inc.
 * Copyright 2010-2019 Tormod Volden and Stefan Schmidt
 * Copyright 2013-2014 Hans Petter Selasky <hps@bitfrost.no>
 *
 * Written by Harald Welte <laforge@openmoko.org>
 *
 * Based on existing code of dfu-programmer-0.4
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <libusb-1.0/libusb.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

#include "portable.h"
#include "dfu.h"
#include "dfu_file.h"
#include "dfu_load.h"
#include "dfu_util.h"
#include "dfuse.h"

int verbose = 0;

struct dfu_if *dfu_root = NULL;

char *match_path = NULL;
int match_vendor = -1;
int match_product = -1;
int match_vendor_dfu = -1;
int match_product_dfu = -1;
int match_config_index = -1;
int match_iface_index = -1;
int match_iface_alt_index = -1;
const char *match_iface_alt_name = NULL;
const char *match_serial = NULL;
const char *match_serial_dfu = NULL;

static int parse_match_value(const char *str, int default_value)
{
	char *remainder;
	int value;

	if (str == NULL) {
		value = default_value;
	} else if (*str == '*') {
		value = -1; /* Match anything */
	} else if (*str == '-') {
		value = 0x10000; /* Impossible vendor/product ID */
	} else {
		value = strtoul(str, &remainder, 16);
		if (remainder == str) {
			value = default_value;
		}
	}
	return value;
}

static void parse_vendprod(const char *str)
{
	const char *comma;
	const char *colon;

	/* Default to match any DFU device in runtime or DFU mode */
	match_vendor = -1;
	match_product = -1;
	match_vendor_dfu = -1;
	match_product_dfu = -1;

	comma = strchr(str, ',');
	if (comma == str) {
		/* DFU mode vendor/product being specified without any runtime
		 * vendor/product specification, so don't match any runtime device */
		match_vendor = match_product = 0x10000;
	} else {
		colon = strchr(str, ':');
		if (colon != NULL) {
			++colon;
			if ((comma != NULL) && (colon > comma)) {
				colon = NULL;
			}
		}
		match_vendor = parse_match_value(str, match_vendor);
		match_product = parse_match_value(colon, match_product);
		if (comma != NULL) {
			/* Both runtime and DFU mode vendor/product specifications are
			 * available, so default DFU mode match components to the given
			 * runtime match components */
			match_vendor_dfu = match_vendor;
			match_product_dfu = match_product;
		}
	}
	if (comma != NULL) {
		++comma;
		colon = strchr(comma, ':');
		if (colon != NULL) {
			++colon;
		}
		match_vendor_dfu = parse_match_value(comma, match_vendor_dfu);
		match_product_dfu = parse_match_value(colon, match_product_dfu);
	}
}

static void parse_serial(char *str)
{
	char *comma;

	match_serial = str;
	comma = strchr(str, ',');
	if (comma == NULL) {
		match_serial_dfu = match_serial;
	} else {
		*comma++ = 0;
		match_serial_dfu = comma;
	}
	if (*match_serial == 0) match_serial = NULL;
	if (*match_serial_dfu == 0) match_serial_dfu = NULL;
}

static int parse_number(char *str, char *nmb)
{
	char *endptr;
	long val;

	errno = 0;
	val = strtol(nmb, &endptr, 0);

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0) || (*endptr != '\0')) {
		errx(EX_SOFTWARE, "Something went wrong with the argument of --%s\n", str);
	}

	if (endptr == nmb) {
		errx(EX_SOFTWARE, "No digits were found from the argument of --%s\n", str);
	}

	return (int)val;
}


/* Walk the device tree and print out DFU devices */
/*
void DFU_list_interfaces(void)
{
	struct dfu_if *pdfu;

	for (pdfu = dfu_root; pdfu != NULL; pdfu = pdfu->next)
		print_dfu_if(pdfu);
}
*/

void usage(void)
{
    printf("stm32_serial_flash [-ex] [-w <filename>]\n");
    printf("    -e : erase\n");
    printf("    -w <file name> : writes device with file <filename>, binary only\n");
    printf("    -x : execute from 0x08000000\n");
    printf("    -p <serial device>: use <serial device> instead/dev/ttyUSB1\n");
    printf("    -u : unprotect\n");
}

#define ADDRESS 0x08000000

int main(int argc, char * argv[])
{
char    c;
int ret , err;
enum mode mode = MODE_NONE;
libusb_context *ctx;
struct dfu_file file;
struct dfu_status status;
unsigned int transfer_size = 0;
const char dfuse_options[32];
unsigned int address = ADDRESS;

	memset(&file, 0, sizeof(file));
    sprintf(dfuse_options,"0x%08x",ADDRESS);
    while ((c = getopt (argc, argv, "w:r:oe")) != -1)
    {
        switch (c)
        {
            case 'e'    :   //DFU_erase(dfudev);
                            break;
            case 'w'    :   file.name= optarg;
                            mode = MODE_DOWNLOAD;
                            break;
            default     :   usage(); return -1;
        }
    }

    match_iface_alt_index = 0;
    switch ( mode )
    {
        case  MODE_DOWNLOAD :
                DFU_load_file(&file, MAYBE_SUFFIX, MAYBE_PREFIX);
                break;
        default : return 0;
    }

    if ((ret = libusb_init(&ctx)))
	{
		printf("Unable to initialize libusb: %s\n", libusb_error_name(ret));
		return -1;
	}

    //probe_devices(ctx);

    //DFU_list_interfaces();

	if (dfu_root == NULL)
	{
        printf("No DFU capable USB device available\n");
		return -1;
	}

	/* We have exactly one device. Its libusb_device is now in dfu_root->dev */

	//printf("Opening DFU capable USB device...\n");
	ret = libusb_open(dfu_root->dev, &dfu_root->dev_handle);
	if (ret || !dfu_root->dev_handle)
	{
		printf("Cannot open device: %s\n", libusb_error_name(ret));
		return -1;
	}

	//printf("ID %04x:%04x\n", dfu_root->vendor, dfu_root->product);
	//printf("Run-time device DFU version %04x\n",libusb_le16_to_cpu(dfu_root->func_dfu.bcdDFUVersion));

    //printf("Claiming USB DFU Runtime Interface...\n");
    ret = libusb_claim_interface(dfu_root->dev_handle, dfu_root->interface);
    if (ret < 0)
    {
        printf("Cannot claim interface %d: %s\n",dfu_root->interface, libusb_error_name(ret));
		return -1;
    }

    ret = libusb_set_interface_alt_setting(dfu_root->dev_handle, dfu_root->interface, 0);
    if (ret < 0)
    {
        printf("Cannot set alt interface zero: %s\n", libusb_error_name(ret));
		return -1;
    }

    printf("Determining device status: ");

    err = DFU_get_status(dfu_root, &status);
    if (err == LIBUSB_ERROR_PIPE) {
        printf("Device does not implement get_status, assuming appIDLE\n");
        status.bStatus = DFU_STATUS_OK;
        status.bwPollTimeout = 0;
        status.bState  = DFU_STATE_appIDLE;
        status.iString = 0;
    } else if (err < 0) {
        errx(EX_IOERR, "error get_status");
    } else {
        printf("state = %s, status = %d\n",dfu_state_to_string(status.bState), status.bStatus);
    }
	printf("DFU mode device DFU version %04x\n",libusb_le16_to_cpu(dfu_root->func_dfu.bcdDFUVersion));

	/*if (dfu_root->func_dfu.bcdDFUVersion == libusb_cpu_to_le16(0x11a))
		dfuse_device = 1;
    */
	/* If not overridden by the user */
    transfer_size = libusb_le16_to_cpu(dfu_root->func_dfu.wTransferSize);
    if (transfer_size)
    {
        printf("Device returned transfer size %i\n",transfer_size);
        if (transfer_size < dfu_root->bMaxPacketSize0)
        {
            transfer_size = dfu_root->bMaxPacketSize0;
            printf("Adjusted transfer size to %i\n", transfer_size);
        }
    }
    else
    {
        printf("Transfer size not specified\n");
        return -1;
    }

    switch (mode)
    {
        case MODE_DOWNLOAD:
            //printf("%s : Address 0x%08x , transfer size %d\n",__FUNCTION__,ADDRESS,transfer_size);
            if (DFU_bin_download(dfu_root, transfer_size, &file,	ADDRESS) < 0)
                return -1;
            break;
        default:
                    errx(EX_IOERR, "Unsupported mode: %u", mode);
                    break;
	}

	libusb_close(dfu_root->dev_handle);
	dfu_root->dev_handle = NULL;
	libusb_exit(ctx);
}
