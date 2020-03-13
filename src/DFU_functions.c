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

void DFU_load_file(struct dfu_file *file)
{
FILE *fp;
/*
	file->size.prefix = 0;
	file->size.suffix = 0;

	file->bcdDFU = 0;
	file->idVendor = 0xffff;
	file->idProduct = 0xffff;
	file->bcdDevice = 0xffff;
	file->lmdfu_address = 0;
*/
	free(file->firmware);
    file->firmware = malloc(BIN_DATA_MAX_SIZE);

    fp = fopen(file->name,"rb");
    if ( fp )
    {
        file->file_len = fread(file->firmware,1,BIN_DATA_MAX_SIZE,fp);
        fclose(fp);
    }
    else
        printf("Could not open file %s for reading\n", file->name);
}


int DFU_bin_download(libusb_device_handle *handle, int xfer_size, struct dfu_file *file, unsigned int start_address)
{
unsigned int dwElementAddress;
unsigned int dwElementSize;
unsigned char *data;
int ret;
	dwElementAddress = start_address;
	dwElementSize = file->file_len;

	data = file->firmware;

	ret = DFU_download_whole_element(handle, dwElementAddress, dwElementSize, data,  xfer_size);
	if (ret != 0)
		return -1;
	return dwElementSize;
}

int DFU_download_whole_element(libusb_device_handle *handle, unsigned int dwElementAddress,unsigned int dwElementSize, unsigned char *data,int xfer_size)
{
int i;
int ret;
int number_of_blocks;
int chunk_size,last_chunk_size;
FILE *fp;
/*
    for (i = dwElementAddress; i < dwElementAddress + dwElementSize + 1; i += ERASE_PAGE_SIZE)
    {
        printf ( "Erasing address 0x%08x\r",i);
        fflush(stdout);
        if ( DFU_erase_page(handle, i) == -1)
            return -1;
    }
    */
    printf ( "Mass erase\r");
    fflush(stdout);
    DFU_mass_erase(handle);

    fp = fopen("dumpfile.bin","wb");
    if ( !fp )
    {
        printf("File dumpfile.bin can't be opened\n");
        exit(0);
    }

    number_of_blocks = (dwElementSize / WRITE_PAGE_SIZE)+1;
    last_chunk_size = dwElementSize - ((dwElementSize / WRITE_PAGE_SIZE)*WRITE_PAGE_SIZE);

	for (i = 0; i < number_of_blocks; i++)
	{
        if ( i == number_of_blocks - 1)
            chunk_size = last_chunk_size;
        else
            chunk_size = WRITE_PAGE_SIZE;
        if ( chunk_size != 0 )
        {
            DFU_set_address(handle, dwElementAddress + i*WRITE_PAGE_SIZE);
            printf("Writing address 0x%08x (%.2f%%)\r",dwElementAddress + i*WRITE_PAGE_SIZE, ((100.0f / dwElementSize) * chunk_size)*(i));
            ret = DFU_download_data(handle, data + i*WRITE_PAGE_SIZE, chunk_size, 2);
            if (ret != chunk_size)
            {
                printf("\n@ %d : Written only %d bytes\n",i,ret);
                return -1;
            }
            fflush(stdout);
            fwrite(data + i,1,chunk_size,fp);
        }
	}
	printf("Write succesfully finished                                           \n");
    fclose(fp);
    ret = DFU_download_end(handle,dwElementAddress);
	return 0;
}

unsigned char read_file[131072];

int DFU_bin_upload(libusb_device_handle *handle, int xfer_size, struct dfu_file *file, unsigned int start_address, unsigned int max_transfers)
{
unsigned int transfers = BIN_DATA_MAX_SIZE , offset=2;
int i;
FILE *fp;

    if ( max_transfers != 0 )
        transfers = max_transfers;

    if ( DFU_set_address(handle, start_address) == -1)
        return -1;
    if ( DFU_go_to_idle(handle) == -1)
        return -1;
    fp = fopen(file->name,"wb");
    if ( !fp )
    {
        printf("File %s can't be opened\n",file->name);
        return -1;
    }

    for(i=0;i<transfers;i+=xfer_size)
    {
        printf ( "Reading at address 0x%08x\r",start_address+i);
        fflush(stdout);
        if ( DFU_upload( handle,xfer_size,offset, &read_file[i] ) == -1)
            return -1;
        offset++;
    }

    fwrite(read_file,1,transfers,fp);
    printf("\nDone\n");
    fclose(fp);
    DFU_go_to_idle(handle);
    return 0;
}

int DFU_get_supported_commands(libusb_device_handle *handle)
{
int status;
    status = DFU_get_state( handle );
    return status;
}
