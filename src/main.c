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
int flash_address , option_bytes_address , otp_address;
int flash_size , option_bytes_size , otp_size;

void usage(void)
{
    printf("stm32_serial_flash [-ex] [-w <filename>] [-r <filename> [ -n <number of bytes to read]] ]\n");
    printf("    -s : target supported commands\n");
    printf("    -e : mass erase\n");
    printf("    -w <file name> : writes device with file <filename>, binary only\n");
    printf("    -r <file name> : reads device and store to file <filename>, binary only\n");
    printf("    -n <number of bytes>: valid only with -r, default is %d\n",BIN_DATA_MAX_SIZE);
    printf("    -o : dump option bytes\n");
    printf("    -u : unprotect device\n");
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

int parse_data(unsigned char *data)
{
int  address,m1,m2,m3,size1,size2,size3;
char name0[32],name1[20],type1K,type1a,type2K,type2a,type3K,type3a;
// Area Name        address    m1*size1,m2*size2,m3*size3

   sscanf( (char * )data,"%s %s /0x%08x/%02d*%03d%c%c,%02d*%03d%c%c,%02d*%03d%c%c",name0,name1,&address,&m1,&size1,&type1K,&type1a,&m2,&size2,&type2K,&type2a,&m3,&size3,&type3K,&type3a);
   //printf("%s %s 0x%08x %d %d %c%c %d %d %c%c %d %d %c%c\n",name0,name1,address,m1,size1,type1K,type1a,m2,size2,type2K,type2a,m3,size3,type3K,type3a);
   if (type1K == 'K')
    size1 *=1024;
   if (type2K == 'K')
    size2 *=1024;
   if (type3K == 'K')
    size3 *=1024;
   return m1*size1+m2*size2+m3*size3;
}

int get_address(unsigned char *data)
{
int  address;
char name0[32],name1[20];
   sscanf( (char *)data,"%s %s /0x%08x/",name0,name1,&address);
   return address;
}

void list_device_characteristics(unsigned char *data , int len )
{
//@Internal Flash  /0x08000000/04*016Kg,01*64Kg,03*128Kg
    if ( data[0] == '@')
    {
        if ( strncmp((char *)data,"@Internal Flash",15) == 0 )
        {
            flash_address = get_address(data);
            flash_size = device_max_transfers = parse_data(data);
            printf("Internal Flash @ 0x%08x , %d Kbytes\n",flash_address,device_max_transfers/1024);
        }
        if ( strncmp((char *)data,"@Option Bytes",13) == 0 )
        {
            option_bytes_address = get_address(data);
            option_bytes_size = parse_data(data);
            printf("Option Bytes   @ 0x%08x , %d bytes\n",option_bytes_address,option_bytes_size);
        }
        if ( strncmp((char *)data,"@OTP Memory",11) == 0 )
        {
            otp_address = get_address(data);
            otp_size = parse_data(data);
            printf("OTP Memory     @ 0x%08x , %d bytes\n",otp_address,otp_size);
        }
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
    while( 1 )
    {
        for(i=0;i<10;i++)
        {
            r = libusb_get_string_descriptor(handle, i, 0, tbuf, sizeof(tbuf));
            if ( r == 255 )
                return 0;
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

    while ((c = getopt (argc, argv, ":w:r:n:esou")) != -1)
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
            case 'o'    :   mode = MODE_READ_OPTIONS;
                            break;
            case 'u'    :   mode = MODE_UNPROTECT;
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
        case  MODE_READ_OPTIONS :
            DFU_read_option_bytes(handle);
            break;
        case MODE_UNPROTECT:
            DFU_unprotect(handle);
            break;
        default:
            printf( "Unsupported mode: %u\n", mode);
            usage();
            break;
	}

    dfu_usb_deInit();
}
