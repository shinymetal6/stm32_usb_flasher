#ifndef DFU_FUNCTIONS_H
#define DFU_FUNCTIONS_H

#include "DFU_usb.h"

#include <stdint.h>

struct dfu_file {
    /* File name */
    const char *name;
    /* Pointer to file loaded into memory */
    uint8_t *firmware;
    /* Different sizes */
    struct {
	int total;
	int prefix;
	int suffix;
    } size;
    /* From prefix fields */
    uint32_t lmdfu_address;
    /* From prefix fields */
    uint32_t prefix_type;

    /* From DFU suffix fields */
    uint32_t dwCRC;
    uint16_t bcdDFU;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
};

void DFU_load_file(struct dfu_file *file);
int DFU_bin_download(struct dfu_if *dif, int xfer_size, struct dfu_file *file, unsigned int start_address);
int DFU_download_whole_element(struct dfu_if *dif, unsigned int dwElementAddress,unsigned int dwElementSize, unsigned char *data,int xfer_size);
const char *DFU_state_to_string( int state );
const char *DFU_status_to_string(int status);

#endif /* DFU_FUNCTIONS_H */

