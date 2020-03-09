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
#include "dfu.h"
#include "usb_dfu.h"
#include "dfu_file.h"
#include "dfuse.h"
#include "dfuse_mem.h"
#include "quirks.h"

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


int DFU_bin_download(struct dfu_if *dif, int xfer_size, struct dfu_file *file, unsigned int start_address)
{
unsigned int dwElementAddress;
unsigned int dwElementSize;
unsigned char *data;
int ret;
	dwElementAddress = start_address;
	dwElementSize = file->size.total - file->size.suffix - file->size.prefix;

	data = file->firmware + file->size.prefix;

	ret = DFU_download_whole_element(dif, dwElementAddress, dwElementSize, data,  xfer_size);
	if (ret != 0)
		return -1;
	return dwElementSize;
}


int DFU_download_whole_element(struct dfu_if *dif, unsigned int dwElementAddress,
			 unsigned int dwElementSize, unsigned char *data,
			 int xfer_size)
{
int i;
int ret;
int page_size;
int chunk_size;

    page_size = 2048;
    printf("\nErase\n");
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
    ret = DFU_download_end(dif,dwElementAddress);
	return 0;
}

const char* dfu_state_to_string( int state )
{
    const char *message;

    switch (state) {
        case STATE_APP_IDLE:
            message = "appIDLE";
            break;
        case STATE_APP_DETACH:
            message = "appDETACH";
            break;
        case STATE_DFU_IDLE:
            message = "dfuIDLE";
            break;
        case STATE_DFU_DOWNLOAD_SYNC:
            message = "dfuDNLOAD-SYNC";
            break;
        case STATE_DFU_DOWNLOAD_BUSY:
            message = "dfuDNBUSY";
            break;
        case STATE_DFU_DOWNLOAD_IDLE:
            message = "dfuDNLOAD-IDLE";
            break;
        case STATE_DFU_MANIFEST_SYNC:
            message = "dfuMANIFEST-SYNC";
            break;
        case STATE_DFU_MANIFEST:
            message = "dfuMANIFEST";
            break;
        case STATE_DFU_MANIFEST_WAIT_RESET:
            message = "dfuMANIFEST-WAIT-RESET";
            break;
        case STATE_DFU_UPLOAD_IDLE:
            message = "dfuUPLOAD-IDLE";
            break;
        case STATE_DFU_ERROR:
            message = "dfuERROR";
            break;
        default:
            message = NULL;
            break;
    }

    return message;
}

static const char *dfu_status_names[] = {
	/* DFU_STATUS_OK */
		"No error condition is present",
	/* DFU_STATUS_errTARGET */
		"File is not targeted for use by this device",
	/* DFU_STATUS_errFILE */
		"File is for this device but fails some vendor-specific test",
	/* DFU_STATUS_errWRITE */
		"Device is unable to write memory",
	/* DFU_STATUS_errERASE */
		"Memory erase function failed",
	/* DFU_STATUS_errCHECK_ERASED */
		"Memory erase check failed",
	/* DFU_STATUS_errPROG */
		"Program memory function failed",
	/* DFU_STATUS_errVERIFY */
		"Programmed memory failed verification",
	/* DFU_STATUS_errADDRESS */
		"Cannot program memory due to received address that is out of range",
	/* DFU_STATUS_errNOTDONE */
		"Received DFU_DNLOAD with wLength = 0, but device does not think that it has all data yet",
	/* DFU_STATUS_errFIRMWARE */
		"Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations",
	/* DFU_STATUS_errVENDOR */
		"iString indicates a vendor specific error",
	/* DFU_STATUS_errUSBR */
		"Device detected unexpected USB reset signalling",
	/* DFU_STATUS_errPOR */
		"Device detected unexpected power on reset",
	/* DFU_STATUS_errUNKNOWN */
		"Something went wrong, but the device does not know what it was",
	/* DFU_STATUS_errSTALLEDPKT */
		"Device stalled an unexpected request"
};


const char *dfu_status_to_string(int status)
{
	if (status > DFU_STATUS_errSTALLEDPKT)
		return "INVALID";
	return dfu_status_names[status];
}



