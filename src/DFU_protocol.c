/*
 * DFU_protocol.c
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

#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "DFU_protocol.h"
#include "DFU_functions.h"

unsigned char   ignore_get_status_errors = 0;

void milli_sleep(unsigned int msec)
{
    usleep(msec*1000);
}

int DFU_download(libusb_device_handle *handle, const unsigned short wLength,unsigned char *data, unsigned short wValue)
{
int status;
	status = libusb_control_transfer(handle,
		 /* bmRequestType */	 LIBUSB_ENDPOINT_OUT |
                                 LIBUSB_REQUEST_TYPE_CLASS |
                                 LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	 DFU_DNLOAD,
		 /* wValue        */	 wValue,
		 /* wIndex        */	 0,
		 /* Data          */	 data,
		 /* wLength       */	 wLength,
					 DFU_TIMEOUT);
	if (status < 0)
		printf("%s: libusb_control_transfer returned %d : %s\n",__FUNCTION__, status,libusb_strerror(status));
	return status;
}

int DFU_upload( libusb_device_handle *handle, const unsigned short wLength,const unsigned short wValue,unsigned char* data )
{
int status;
	status = libusb_control_transfer(handle,
		 /* bmRequestType */	LIBUSB_ENDPOINT_IN |
                                LIBUSB_REQUEST_TYPE_CLASS |
                                LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	DFU_UPLOAD,
		 /* wValue        */	wValue,
		 /* wIndex        */	0,
		 /* Data          */	data,
		 /* wLength       */	wLength,
					 DFU_TIMEOUT);
	if (status < 0)
	{
		printf("%s: libusb_control_transfer returned %d : %s\n",__FUNCTION__, status,libusb_strerror(status));
        return -1;
	}
	return status;
}

int DFU_get_state( libusb_device_handle *handle )
{
int status , length = 4 , i;
unsigned char buffer[4];
	status = libusb_control_transfer(handle,
		 /* bmRequestType */	LIBUSB_ENDPOINT_IN |
                                LIBUSB_REQUEST_TYPE_CLASS |
                                LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	DFU_UPLOAD,
		 /* wValue        */	0,
		 /* wIndex        */	0,
		 /* Data          */	buffer,
		 /* wLength       */	length,
					 DFU_TIMEOUT);
	if (status < 0)
	{
		printf("%s: libusb_control_transfer returned %d : %s\n",__FUNCTION__, status,libusb_strerror(status));
		return status;
	}
	printf("Supported commands : ");
	for(i=0;i<length;i++)
        printf("0x%02x ", buffer[i]);
    printf ("\n");
	status= buffer[0]<<24 | buffer[1]<<16 | buffer[2]<<8|buffer[3];
	return status;
}

int DFU_get_status( libusb_device_handle *handle, struct dfu_status *status )
{
unsigned char buffer[6];
int result;

    /* Initialize the status data structure */
    status->bStatus       = DFU_STATUS_ERROR_UNKNOWN;
    status->bwPollTimeout = 0;
    status->bState        = STATE_DFU_ERROR;
    status->iString       = 0;

	result = libusb_control_transfer(handle,
          /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
          /* bRequest      */ DFU_GETSTATUS,
          /* wValue        */ 0,
          /* wIndex        */ 0,
          /* Data          */ buffer,
          /* wLength       */ 6,
                              DFU_TIMEOUT );
	if (result < 0)
	{
        if ( ignore_get_status_errors == 0 )
            printf("%s: libusb_control_transfer returned %d : %s\n",__FUNCTION__, result,libusb_strerror(result));
		return result;
	}

    if( result == 6 )
    {
        status->bStatus = buffer[0];
        status->bwPollTimeout = ((buffer[3] << 16) & 0xff) | ((buffer[2] << 8) & 0xff) | (buffer[1] & 0xff);
        status->bState  = buffer[4];
        status->iString = buffer[5];
        if (status->bwPollTimeout != 0)
        {
            //printf("\nRequested Delay %d mSec. ( 0x%08x , 0x%02x%02x%02x )\n",status->bwPollTimeout,status->bwPollTimeout,((buffer[3] << 16) & 0xff) , ((buffer[2] << 8) & 0xff) , (buffer[1] & 0xff));
            milli_sleep(status->bwPollTimeout);
        }
        result = status->bStatus;
    }
    return result;
}

int DFU_clear_status( libusb_device_handle *handle )
{
    return libusb_control_transfer( handle,
        /* bmRequestType */ LIBUSB_ENDPOINT_OUT| LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        /* bRequest      */ DFU_CLRSTATUS,
        /* wValue        */ 0,
        /* wIndex        */ 0,
        /* Data          */ NULL,
        /* wLength       */ 0,
                            DFU_TIMEOUT );
}

int DFU_abort( libusb_device_handle *handle )
{
    return libusb_control_transfer( handle,
        /* bmRequestType */ LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        /* bRequest      */ DFU_ABORT,
        /* wValue        */ 0,
        /* wIndex        */ 0,
        /* Data          */ NULL,
        /* wLength       */ 0,
                            DFU_TIMEOUT );
}

int DFU_go_to_idle(libusb_device_handle *handle)
{
int ret;
struct dfu_status dst;

	ret = DFU_abort(handle);
	if (ret < 0)
	{
		printf("Error sending dfu abort request\n");
		return -1;
	}
	ret = DFU_get_status(handle, &dst);
	if (ret < 0)
	{
		printf("Error during abort get_status\n");
		return -1;
	}
	if (dst.bState != DFU_STATE_dfuIDLE)
	{
		printf("Failed to enter idle state on abort\n");
		return -1;
	}
	return ret;
}

int DFU_download_data(libusb_device_handle *handle, unsigned char *data, int size,int transaction)
{
int bytes_sent;
struct dfu_status dst;
int ret;

    if ( size == 0 )
    {
        printf("\n\n\nsize = 0\n");
        return -1;
    }
	ret = DFU_download(handle, size, size ? data : NULL, transaction);
	if (ret < 0)
	{
		printf("Error during download\n");
		return ret;
	}
	bytes_sent = ret;

	do {
		ret = DFU_get_status(handle, &dst);
		if (ret < 0) {
			printf("Error during download get_status\n");
			return ret;
		}
	} while (dst.bState != DFU_STATE_dfuDNLOAD_IDLE &&
		 dst.bState != DFU_STATE_dfuERROR &&
		 dst.bState != DFU_STATE_dfuMANIFEST &&
		 dst.bState == DFU_STATE_dfuDNBUSY);
		 /*!(dfuse_will_reset && (dst.bState == DFU_STATE_dfuDNBUSY)));*/

	if (dst.bState == DFU_STATE_dfuMANIFEST)
			printf("Transitioning to dfuMANIFEST state\n");

	if (dst.bStatus != DFU_STATUS_OK) {
		printf("Failed with 0x%02x\n",dst.bStatus);
		return -1;
	}
	return bytes_sent;
}

int DFU_general_command(libusb_device_handle *handle, unsigned int address , unsigned char command, unsigned int cmdlen)
{
unsigned char buf[5];
int ret;
struct dfu_status dst;

    buf[0] = command;
	buf[1] = address & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = (address >> 16) & 0xff;
	buf[4] = (address >> 24) & 0xff;

	ret = DFU_download(handle, cmdlen, buf, 0);
	if (ret < 0)
	{
        printf("%s : Error 0 during command 0x%02x\n",__FUNCTION__,command);
		return -1;
	}
    ret = DFU_get_status(handle, &dst);
    if (ret < 0)
    {
        if ( ignore_get_status_errors == 0 )
            printf("%s : Error 1 during command 0x%02x with len %d\n",__FUNCTION__,command,cmdlen);
        return -1;
    }
	if (dst.bState != STATE_DFU_DOWNLOAD_BUSY)
	{
        printf("%s : with command 0x%02x expecting STATE_DFU_DOWNLOAD_BUSY ( 0x%02x ), received 0x%02x\n",__FUNCTION__,command,STATE_DFU_DOWNLOAD_BUSY,dst.bStatus);
        return -1;
	}
    //printf("%s : received %d for timeout\n",__FUNCTION__,dst.bwPollTimeout);
    ret = DFU_get_status(handle, &dst);
    if (ret < 0)
    {
        printf("%s : Error 2 during command 0x%02x\n",__FUNCTION__,command);
        return -1;
    }
	if (dst.bStatus != DFU_STATUS_OK)
	{
        printf("%s : with command 0x%02x expecting DFU_STATUS_OK ( 0x%02x ), received 0x%02x\n",__FUNCTION__,command,DFU_STATUS_OK,dst.bStatus);
        return -1;
	}
	return 0;
}

int DFU_set_address(libusb_device_handle *handle, unsigned int address)
{
    return DFU_general_command( handle,  address , DFU_SET_ADDRESS_CMD , DFU_SET_ADDRESS_CMD_LEN);
}

int DFU_erase_page(libusb_device_handle *handle, unsigned int address)
{
    return DFU_general_command( handle,  address , DFU_ERASE_CMD , DFU_ERASE_CMD_LEN);
}

int DFU_mass_erase(libusb_device_handle *handle)
{
    return DFU_general_command( handle,  0 , DFU_MASS_ERASE_CMD , DFU_MASS_ERASE_CMD_LEN);
}

int DFU_download_end(libusb_device_handle *handle, unsigned int address)
{
    DFU_set_address(handle, address);
    ignore_get_status_errors = 1;
    DFU_general_command( handle,  0 , DFU_EXIT_DFU_CMD , DFU_EXIT_DFU_CMD_LEN);
    printf("Target should be running\n");
    ignore_get_status_errors = 0;
    return 0;
}
