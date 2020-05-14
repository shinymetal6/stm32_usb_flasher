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
#include "elf_int.h"


void DFU_load_file(struct dfu_file *file)
{
FILE    *fp;
char    out_file_name[32],in_file_name[32];
char    file_type[32];

    sprintf(file_type,"it's a binary file");
    sprintf(in_file_name,"%s",file->name);
    sprintf(out_file_name,"translated.bin");
    free(file->firmware);
    file->firmware = malloc(BIN_DATA_MAX_SIZE);

    /* try with elf interpreter */
    if ( load_elf(file->name, out_file_name , 0) == 0 )
    {
        sprintf(file_type,"it's an ELF file");
        sprintf(file->name,"%s",out_file_name);
    }

    printf("%s %s\n",in_file_name, file_type);
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
#ifdef  SINGLE_PAGE_ERASE
    for (i = dwElementAddress; i < dwElementAddress + dwElementSize + 1; i += ERASE_PAGE_SIZE)
    {
        printf ( "Erasing address 0x%08x\r",i);
        fflush(stdout);
        if ( DFU_erase_page(handle, i) == -1)
            return -1;
    }
#else
    printf ( "Mass erase\n");
    fflush(stdout);
    milli_sleep(100);
	DFU_clear_status(handle);
	milli_sleep(100);
    DFU_mass_erase(handle);
#endif
	milli_sleep(100);
	DFU_clear_status(handle);
	milli_sleep(100);
    fp = fopen("dumpfile.bin","wb");
    if ( !fp )
    {
        printf("File dumpfile.bin can't be opened\n");
        exit(0);
    }
    printf ( "Starting write\n");

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
extern  int flash_address , option_bytes_address , otp_address;
extern  int flash_size , option_bytes_size , otp_size;

#define OPTION_BYTES_SIZE   48
unsigned int optbytes[OPTION_BYTES_SIZE];
int DFU_read_option_bytes(libusb_device_handle *handle )
{
int i;

    if ( DFU_set_address(handle, option_bytes_address) == -1)
    {
        printf("%s : error @ DFU_set_address\n",__FUNCTION__);
        return -1;
    }
    if ( DFU_go_to_idle(handle) == -1)
    {
        printf("%s : error @ DFU_go_to_idle\n",__FUNCTION__);
        return -1;
    }

    if ( DFU_upload( handle,option_bytes_size,2, (unsigned char* )optbytes ) == -1)
    {
        printf("%s : error @ DFU_upload\n",__FUNCTION__);
        return -1;
    }

    for(i=0;i<option_bytes_size;i+=8)
        printf("0x%08x : 0x%08x\n",option_bytes_address+i,optbytes[i]);
    DFU_go_to_idle(handle);
    return 0;
}

int DFU_get_supported_commands(libusb_device_handle *handle)
{
int status;
    status = DFU_get_state( handle );
    return status;
}
