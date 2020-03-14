/*
 * stm32_usb_flasher
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

#include "DFU_protocol.h"
#include "DFU_functions.h"

struct libusb_device_handle *handle;
int device_max_transfers;

void usage(void)
{
    printf("stm32_serial_flash [-ex] [-w <filename>] [-r <filename> [ -n <number of bytes to read]] ]\n");
    printf("    -s : target supported commands\n");
    printf("    -e : mass erase\n");
    printf("    -w <file name> : writes device with file <filename>, binary only\n");
    printf("    -r <file name> : reads device and store to file <filename>, binary only\n");
    printf("    -n <number of bytes>: valid only with -r, default is %d\n",BIN_DATA_MAX_SIZE);
}

static void print_devs(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0;

	while ((dev = devs[i++]) != NULL)
	{
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
		{
			fprintf(stderr, "failed to get device descriptor");
			return;
		}
        if ( (desc.idVendor == VID) && (desc.idProduct == PID))
            printf("Found device %04x:%04x %d configurations\n",	desc.idVendor, desc.idProduct, desc.bNumConfigurations);
	}
}


void spr(unsigned char *tbuf )
{
int i,outlen;
    outlen = tbuf[0] - 2;
    outlen +=2;
    outlen = tbuf[0];

    printf("HEX:\n");

    for(i=2;i<outlen;i+=2)
    {
        printf("0x%02x ",tbuf[i]);
    }
    printf("\nASCII:\n");

    for(i=2;i<outlen;i+=2)
    {
        printf("%c",tbuf[i]);
    }
    printf("\n");
}


int get_size(unsigned char *line_in , unsigned int *address)
{
int i,k;
int first=0,second=0,star;
//char addr[12]
char size[4];
    for(i=0;i<strlen((char * )line_in);i++)
    {
        if (( line_in[i] == '/') && (first != 0))
            second = i;
        if (( line_in[i] == '/') && (first == 0))
            first=i+3;
        if ( line_in[i] == '*')
            star=i;
    }
    /*
    k=0;
    for(i=first;i<second;i++)
    {
        addr[k] = line_in[i];
        k++;
    }
    addr[k] = 0;
    */
    k=0;
    for(i=second+1;i<star;i++)
    {
        size[k] = line_in[i];
        k++;
    }
    size[k] = 0;
    *address = 0x08000000;
    return atoi(size);
}


void list_device_characteristics(unsigned char *data , int len )
{
unsigned int address;
    if ( strncmp(data,"@Internal Flash",15) == 0 )
    {
        device_max_transfers = get_size(data,&address) * 1024;
        printf("Internal Flash : %d bytes @ 0x%08x\n",device_max_transfers,address);
    }
}

int DFU_describe_interface(void)
{
int     cnt,r,i,j,k;
libusb_device **devs;
unsigned char tbuf[255] , data[255];

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		libusb_exit(NULL);
		return -1;
	}
    print_devs(devs);
    r = 0;
    while( r < 255 )
    {
        for(i=0;i<10;i++)
        {
            r = libusb_get_string_descriptor(handle, i, 0, tbuf, sizeof(tbuf));
            if (( r < 255 ) && (r > 4 ))
            {
                k = 0;
                bzero(data,sizeof(data));
                for(j=2;j<tbuf[0];j+=2)
                {
                    data[k] = tbuf[j];
                    k++;
                }
                list_device_characteristics(data,tbuf[0]);
            }
        }
    }
    return 0;
}

int dfu_usb_init(void)
{
	libusb_init(NULL);
	if((handle = libusb_open_device_with_vid_pid(NULL, VID, PID)) == NULL)
	{
		printf("Device not found or not in DFU mode\n");
		return -1;
	}
	libusb_claim_interface(handle, 0);
	libusb_detach_kernel_driver(handle, 0);

    printf("Found device 0x%04x:0x%04x\n",VID, PID);
	return 0;
}

static int dfu_usb_deInit()
{
	if(handle != NULL)
		libusb_release_interface (handle, 0);
	libusb_exit(NULL);
	return 0;
}

int main(int argc, char * argv[])
{
char    c;
int max_transfers=0;
enum mode mode = MODE_NONE;
struct dfu_file file;
unsigned int address = ADDRESS;


	memset(&file, 0, sizeof(file));

    while ((c = getopt (argc, argv, ":w:r:n:es")) != -1)
    {
        switch (c)
        {
            case 'e'    :   mode = MODE_MASS_ERASE;
                            break;
            case 's'    :   mode = MODE_SUPPORTED_COMMANDS;
                            break;
            case 'w'    :   file.name= optarg;
                            mode = MODE_DOWNLOAD;
                            break;
            case 'n'    :   max_transfers = atoi(optarg);
                            break;
            case 'r'    :   file.name= optarg;
                            mode = MODE_UPLOAD;
                            break;
            default     :   usage(); return -1;
        }
    }

    if ( argc == 1 )
    {
        usage();
        return -1;
    }

    if ( dfu_usb_init() == -1 )
        return -1;
    DFU_describe_interface();

    if ( max_transfers != 0 )
        device_max_transfers = max_transfers;

    switch (mode)
    {
        case MODE_DOWNLOAD:
            DFU_load_file(&file);
            DFU_bin_download(handle, WRITE_PAGE_SIZE, &file, address);
            break;
        case MODE_MASS_ERASE:
            if ( DFU_mass_erase(handle) != -1 )
                printf( "Device erased\n");
            break;
        case  MODE_UPLOAD :
            DFU_bin_upload(handle, WRITE_PAGE_SIZE, &file, address,device_max_transfers);
            break;
        case  MODE_SUPPORTED_COMMANDS :
            DFU_get_supported_commands(handle);
            break;
        default:
            printf( "Unsupported mode: %u\n", mode);
            usage();
            break;
	}

    dfu_usb_deInit();
}
