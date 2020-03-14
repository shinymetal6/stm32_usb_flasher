#ifndef DFU_FUNCTIONS_H
#define DFU_FUNCTIONS_H

#include <stdint.h>
struct dfu_file {
    char *name;
    uint8_t *firmware;
    int file_len;
};

void DFU_load_file(struct dfu_file *file);
int DFU_bin_download(libusb_device_handle *handle, int xfer_size, struct dfu_file *file, unsigned int start_address);
int DFU_bin_upload(libusb_device_handle *handle, int xfer_size, struct dfu_file *file, unsigned int start_address, unsigned int max_transfers);
int DFU_download_whole_element(libusb_device_handle *handle, unsigned int dwElementAddress,unsigned int dwElementSize, unsigned char *data,int xfer_size);
int DFU_get_supported_commands(libusb_device_handle *handle);

#endif /* DFU_FUNCTIONS_H */

