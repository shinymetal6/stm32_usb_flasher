/*
 * DFU_functions.c
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
#include "DFU_usb.h"

void DFU_load_file(struct dfu_file *file)
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
    file->firmware = malloc(BIN_DATA_MAX_SIZE);

    fp = fopen(file->name,"rb");
    if ( fp )
    {
        file->size.total = fread(file->firmware,1,BIN_DATA_MAX_SIZE,fp);
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

int DFU_download_whole_element(struct dfu_if *dif, unsigned int dwElementAddress,unsigned int dwElementSize, unsigned char *data,int xfer_size)
{
int i;
int ret;
int page_size;
int chunk_size;
FILE *fp;

    page_size = 2048;
    printf("\nErase\n");
    for (i = dwElementAddress; i < dwElementAddress + dwElementSize + 1; i += page_size)
    {
        printf ( "Erasing address 0x%08x\r",i);
        DFU_erase_page(dif, i);
        fflush(stdout);
    }
	printf("\nDownload\n");

    fp = fopen("dumpfile.bin","wb");
    if ( !fp )
    {
        printf("File dumpfile.bin can't be opened\n");
        exit(0);
    }
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
        fwrite(data + i,1,chunk_size,fp);
	}
	printf("\nDone\n");
    fclose(fp);
    ret = DFU_download_end(dif,dwElementAddress);
	return 0;
}

const char *DFU_state_to_string( int state )
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


const char *DFU_status_to_string(int status)
{
	if (status > DFU_STATUS_errSTALLEDPKT)
		return "INVALID";
	return dfu_status_names[status];
}

int DFU_bin_upload(struct dfu_if *dif, int xfer_size, struct dfu_file *file, unsigned int start_address, unsigned int max_transfers)
{
unsigned int transfers = BIN_DATA_MAX_SIZE;
unsigned int blocks;
int i;
FILE *fp;

    fp = fopen(file->name,"wb");
    if ( !fp )
    {
        printf("File %s can't be opened\n",file->name);
        return -1;
    }

    if ( max_transfers != 0 )
        transfers = max_transfers;
    blocks = transfers/xfer_size;

    file->firmware = malloc(transfers);
    free(file->firmware);

    if ( DFU_set_address(dif, start_address) == -1)
        return -1;
    if ( DFU_go_to_idle(dif) == -1)
        return -1;
    for(i=0;i<blocks;i++)
    {
        printf ( "Reading at address 0x%08x\r",start_address + blocks*xfer_size);
        if ( DFU_upload( dif,xfer_size,i+2,file->firmware+i*xfer_size ) == -1)
            return -1;
        fwrite(file->firmware+i*xfer_size,1,xfer_size,fp);
    }
    printf("\nDone\n");
    fclose(fp);
    DFU_go_to_idle(dif);
    return 0;
}
