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

#include "portable.h"
#include "dfu_file.h"
#include "portable.h"
#include "dfu.h"
#include "quirks.h"

#define DFU_TIMEOUT 5000
void DFU_load_file(struct dfu_file *file, enum suffix_req check_suffix, enum prefix_req check_prefix)
{
FILE *fp;

	file->size.prefix = 0;
	file->size.suffix = 0;

	file->bcdDFU = 0;
	file->idVendor = 0xffff; /* wildcard value */
	file->idProduct = 0xffff; /* wildcard value */
	file->bcdDevice = 0xffff; /* wildcard value */
	file->lmdfu_address = 0;

	free(file->firmware);
    file->firmware = malloc(131072);

    fp = fopen(file->name,"rb");
    if ( fp )
    {
        file->size.total = fread(file->firmware,1,131072,fp);
        fclose(fp);
    }
    else
        printf("Could not open file %s for reading\n", file->name);
}

int DFU_download(struct dfu_if *dif, const unsigned short length,
		   unsigned char *data, unsigned short transaction)
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
		errx(EX_IOERR, "%s: libusb_control_transfer returned %d",
			__FUNCTION__, status);
	}
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

int DFU_bin_download(struct dfu_if *dif, int xfer_size, struct dfu_file *file, unsigned int start_address)
{
unsigned int dwElementAddress;
unsigned int dwElementSize;
unsigned char *data;
int ret;
    printf("%s : enter\n",__FUNCTION__);
	dwElementAddress = start_address;
	dwElementSize = file->size.total - file->size.suffix - file->size.prefix;

	printf("%s : Downloading to address = 0x%08x, size = %i\n", __FUNCTION__, dwElementAddress, dwElementSize);

	data = file->firmware + file->size.prefix;

	ret = DFU_download_whole_element(dif, dwElementAddress, dwElementSize, data,  xfer_size);
	if (ret != 0)
		return -1;

	printf("File downloaded successfully\n");
	return dwElementSize;
}

extern  unsigned int last_erased_page;
extern  struct memsegment *mem_layout;
extern  unsigned int dfuse_address;
extern  unsigned int dfuse_address_present;
extern  unsigned int dfuse_length;
extern  int dfuse_force;
extern  int dfuse_leave;
extern  int dfuse_unprotect;
extern  int dfuse_mass_erase;
extern  int dfuse_will_reset;

#include "portable.h"
#include "dfu.h"
#include "usb_dfu.h"
#include "dfu_file.h"
#include "dfuse.h"
#include "dfuse_mem.h"
#include "quirks.h"

int DFU_download_whole_element(struct dfu_if *dif, unsigned int dwElementAddress,
			 unsigned int dwElementSize, unsigned char *data,
			 int xfer_size)
{
int i;
int ret;
int page_size;
int chunk_size;

    page_size = 2048;
    printf("\Erase\n");
    for (i = dwElementAddress; i < dwElementAddress + dwElementSize + 1; i += page_size)
    {
        printf ( "Erasing address 0x%08x\r",i);
        DFU_erase_page(dif, i);
        fflush(stdout);
    }
	printf("\nDownload\n");

	/* Second pass: Write data to (erased) pages */
	for (i = 0; i < (int)dwElementSize; i += xfer_size)
	{
		chunk_size = xfer_size;
		/* check if this is the last chunk */
		if (i + chunk_size > (int)dwElementSize)
			chunk_size = dwElementSize - i;
		DFU_set_address(dif, dwElementAddress + i);
        printf ( "Writing address 0x%08x\r",dwElementAddress + i);
		ret = DFU_download_data(dif, data + i, chunk_size, 2);
		if (ret != chunk_size)
		{
            printf("\nWritten only %d bytes\n",ret);
			return -1;
		}
        fflush(stdout);
	}
	printf("\nDone\n");
    ret = DFU_download_data(dif, data + i, 0, 2);
	return 0;
}

int DFU_erase_page(struct dfu_if *dif, unsigned int address)
{
unsigned char buf[5];
int length;
int ret;
struct dfu_status dst;
int firstpoll = 1;
int zerotimeouts = 0;

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
				       dfu_state_to_string(dst.bState), dst.bStatus,
				       dfu_status_to_string(dst.bStatus));
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
		/* wait while command is executed */

		milli_sleep(dst.bwPollTimeout);
		/* Workaround for e.g. Black Magic Probe getting stuck */
		if (dst.bwPollTimeout == 0) {
			if (++zerotimeouts == 100)
				errx(EX_IOERR, "Device stuck after special command request");
		} else {
			zerotimeouts = 0;
		}
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
            printf("%s : Error 3 during erase\n",__FUNCTION__);
	}
	return ret;
}

int DFU_set_address(struct dfu_if *dif, unsigned int address)
{
unsigned char buf[5];
int length;
int ret;
struct dfu_status dst;
int firstpoll = 1;
int zerotimeouts = 0;

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
				       dfu_state_to_string(dst.bState), dst.bStatus,
				       dfu_status_to_string(dst.bStatus));
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
		/* wait while command is executed */

		milli_sleep(dst.bwPollTimeout);
		/* Workaround for e.g. Black Magic Probe getting stuck */
		if (dst.bwPollTimeout == 0) {
			if (++zerotimeouts == 100)
				errx(EX_IOERR, "Device stuck after special command request");
		} else {
			zerotimeouts = 0;
		}
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
            printf("%s : Error 3 during set_address\n",__FUNCTION__);
	}
	return ret;
}

int DFU_download_data(struct dfu_if *dif, unsigned char *data, int size,int transaction)
{
	int bytes_sent;
	struct dfu_status dst;
	int ret;

	ret = DFU_download(dif, size, size ? data : NULL, transaction);
	if (ret < 0) {
		errx(EX_IOERR, "Error during download");
		return ret;
	}
	bytes_sent = ret;

	do {
		ret = DFU_get_status(dif, &dst);
		if (ret < 0) {
			errx(EX_IOERR, "Error during download get_status");
			return ret;
		}
		//milli_sleep(dst.bwPollTimeout);
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
		       dfu_state_to_string(dst.bState), dst.bStatus,
		       dfu_status_to_string(dst.bStatus));
		return -1;
	}
	return bytes_sent;
}

