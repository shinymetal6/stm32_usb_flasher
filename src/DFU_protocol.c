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

/*
#include "portable.h"
#include "dfu_file.h"
#include "portable.h"
//#include "dfu.h"
#include "quirks.h"
*/

#include "DFU_usb.h"
#include "DFU_protocol.h"
#include "DFU_functions.h"

#define DFU_TIMEOUT 5000


int DFU_download(struct dfu_if *dif, const unsigned short length,unsigned char *data, unsigned short transaction)
{
	int status;

	status = libusb_control_transfer(dif->dev_handle,
		 /* bmRequestType */	 LIBUSB_ENDPOINT_OUT |
					 LIBUSB_REQUEST_TYPE_CLASS |
					 LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	 DFU_DNLOAD,
		 /* wValue        */	 transaction,
		 /* wIndex        */	 dif->interface,
		 /* Data          */	 data,
		 /* wLength       */	 length,
					 DFU_TIMEOUT);
	if (status < 0) {
		printf("%s: libusb_control_transfer returned %d\n",__FUNCTION__, status);
	}
	return status;
}

int DFU_upload( libusb_device_handle *device,const unsigned short interface,const unsigned short length,const unsigned short transaction,unsigned char* data )
{
    int status;

    status = libusb_control_transfer( device,
          /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
          /* bRequest      */ DFU_UPLOAD,
          /* wValue        */ transaction,
          /* wIndex        */ interface,
          /* Data          */ data,
          /* wLength       */ length,
                              DFU_TIMEOUT );
    return status;
}

int DFU_get_status( struct dfu_if *dif, struct dfu_status *status )
{
unsigned char buffer[6];
int result;
struct timespec req;

    req.tv_sec = 0;
    req.tv_nsec = 10000000;
    nanosleep(&req, NULL);

    /* Initialize the status data structure */
    status->bStatus       = DFU_STATUS_ERROR_UNKNOWN;
    status->bwPollTimeout = 0;
    status->bState        = STATE_DFU_ERROR;
    status->iString       = 0;

    result = libusb_control_transfer( dif->dev_handle,
          /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
          /* bRequest      */ DFU_GETSTATUS,
          /* wValue        */ 0,
          /* wIndex        */ dif->interface,
          /* Data          */ buffer,
          /* wLength       */ 6,
                              DFU_TIMEOUT );

    if( 6 == result )
    {
        status->bStatus = buffer[0];
        status->bwPollTimeout = ((0xff & buffer[3]) << 16) | ((0xff & buffer[2]) << 8)  | (0xff & buffer[1]);
        status->bState  = buffer[4];
        status->iString = buffer[5];
        if (status->bwPollTimeout != 0)
        {
            req.tv_sec = 0;
            req.tv_nsec = status->bwPollTimeout * 1000000;
            if (0 > nanosleep(&req, NULL))
            {
                printf("dfu_get_status: nanosleep failed");
                return -1;
            }
        }
    }
    return result;
}

int DFU_clear_status( libusb_device_handle *device,const unsigned short interface )
{
    return libusb_control_transfer( device,
        /* bmRequestType */ LIBUSB_ENDPOINT_OUT| LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        /* bRequest      */ DFU_CLRSTATUS,
        /* wValue        */ 0,
        /* wIndex        */ interface,
        /* Data          */ NULL,
        /* wLength       */ 0,
                            DFU_TIMEOUT );
}

int DFU_download_data(struct dfu_if *dif, unsigned char *data, int size,int transaction)
{
int bytes_sent;
struct dfu_status dst;
int ret;

    if ( size == 0 )
    {
        printf("\n\n\nsize = 0\n");
        return 0;
    }
	ret = DFU_download(dif, size, size ? data : NULL, transaction);
	if (ret < 0) {
		printf("Error during download\n");
		return ret;
	}
	bytes_sent = ret;

	do {
		ret = DFU_get_status(dif, &dst);
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
		printf(" failed!\n");
		printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
		       DFU_state_to_string(dst.bState), dst.bStatus,
		       DFU_status_to_string(dst.bStatus));
		return -1;
	}
	return bytes_sent;
}

int DFU_set_address(struct dfu_if *dif, unsigned int address)
{
unsigned char buf[5];
int length;
int ret;
struct dfu_status dst;
int firstpoll = 1;

    buf[0] = 0x21;	/* Set Address Pointer command */
    length = 5;
	buf[1] = address & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = (address >> 16) & 0xff;
	buf[4] = (address >> 24) & 0xff;

	ret = DFU_download(dif, length, buf, 0);
	if (ret < 0) {
		printf("%s : Error 0 during set_address\n",__FUNCTION__);
	}
	do {
		ret = DFU_get_status(dif, &dst);
		if (ret < 0) {
            printf("%s : Error 1 during set_address\n",__FUNCTION__);
		}
		if (firstpoll) {
			firstpoll = 0;
			if (dst.bState != DFU_STATE_dfuDNBUSY) {
				printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
				       DFU_state_to_string(dst.bState), dst.bStatus,
				       DFU_status_to_string(dst.bStatus));
                printf("%s : Error 2 during set_address\n",__FUNCTION__);

			}
			/* STM32F405 lies about mass erase timeout */
			/*
			if (command == MASS_ERASE && dst.bwPollTimeout == 100) {
				dst.bwPollTimeout = 35000;
				printf("Setting timeout to 35 seconds\n");
			}
			*/
		}
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
            printf("%s : Error 3 during set_address\n",__FUNCTION__);
	}
	return ret;
}


int DFU_erase_page(struct dfu_if *dif, unsigned int address)
{
unsigned char buf[5];
int length;
int ret;
struct dfu_status dst;
int firstpoll = 1;

    buf[0] = 0x41;	/* Erase command */
    length = 5;
	buf[1] = address & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = (address >> 16) & 0xff;
	buf[4] = (address >> 24) & 0xff;

	ret = DFU_download(dif, length, buf, 0);
	if (ret < 0) {
		printf("%s : Error 0 during erase\n",__FUNCTION__);
	}
	do {
		ret = DFU_get_status(dif, &dst);
		if (ret < 0) {
            printf("%s : Error 1 during erase\n",__FUNCTION__);
		}
		if (firstpoll) {
			firstpoll = 0;
			if (dst.bState != DFU_STATE_dfuDNBUSY) {
				printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
				       DFU_state_to_string(dst.bState), dst.bStatus,
				       DFU_status_to_string(dst.bStatus));
                printf("%s : Error 2 during erase\n",__FUNCTION__);

			}
			/* STM32F405 lies about mass erase timeout */
			/*
			if (command == MASS_ERASE && dst.bwPollTimeout == 100) {
				dst.bwPollTimeout = 35000;
				printf("Setting timeout to 35 seconds\n");
			}
			*/
		}
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
            printf("%s : Error 3 during erase\n",__FUNCTION__);
	}
	return ret;
}

int DFU_download_end(struct dfu_if *dif,unsigned int address)
{
int ret;
struct dfu_status dst;

    printf("Application started from address 0x%08x\n\n",address);
    DFU_set_address(dif, address);
	ret = DFU_download(dif, 0, NULL, 0x01);
	if (ret < 0)
	{
		printf("Error during download\n");
		return ret;
	}

	do {
		ret = DFU_get_status(dif, &dst);
		return ret;
		if (ret < 0)
		{
			printf("Error during download get_status\n");
			return ret;
		}
		if ( dst.bState == DFU_STATE_dfuDNLOAD_IDLE )
            printf("dst.bState = DFU_STATE_dfuDNLOAD_IDLE\n");
		if ( dst.bState == DFU_STATE_dfuIDLE )
            printf("dst.bState = DFU_STATE_dfuIDLE\n");
        printf("dst.bState = 0x%04x\n",dst.bState);

	} while (dst.bState != DFU_STATE_dfuDNLOAD_IDLE &&
		 dst.bState != DFU_STATE_dfuERROR &&
		 dst.bState != DFU_STATE_dfuMANIFEST &&
		 dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bState == DFU_STATE_dfuMANIFEST)
			printf("Transitioning to dfuMANIFEST state\n");

	if (dst.bStatus != DFU_STATUS_OK) {
		printf(" failed!\n");
		printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
		       DFU_state_to_string(dst.bState), dst.bStatus,
		       DFU_status_to_string(dst.bStatus));
		return -1;
	}
	return ret;
}

