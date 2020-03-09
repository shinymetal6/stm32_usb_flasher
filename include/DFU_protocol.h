#ifndef DFU_PROTOCOL_H
#define DFU_PROTOCOL_H

#include "DFU_usb.h"

/* DFU states */
#define STATE_APP_IDLE                  0x00
#define STATE_APP_DETACH                0x01
#define STATE_DFU_IDLE                  0x02
#define STATE_DFU_DOWNLOAD_SYNC         0x03
#define STATE_DFU_DOWNLOAD_BUSY         0x04
#define STATE_DFU_DOWNLOAD_IDLE         0x05
#define STATE_DFU_MANIFEST_SYNC         0x06
#define STATE_DFU_MANIFEST              0x07
#define STATE_DFU_MANIFEST_WAIT_RESET   0x08
#define STATE_DFU_UPLOAD_IDLE           0x09
#define STATE_DFU_ERROR                 0x0a


/* DFU status */
#define DFU_STATUS_OK                   0x00
#define DFU_STATUS_ERROR_TARGET         0x01
#define DFU_STATUS_ERROR_FILE           0x02
#define DFU_STATUS_ERROR_WRITE          0x03
#define DFU_STATUS_ERROR_ERASE          0x04
#define DFU_STATUS_ERROR_CHECK_ERASED   0x05
#define DFU_STATUS_ERROR_PROG           0x06
#define DFU_STATUS_ERROR_VERIFY         0x07
#define DFU_STATUS_ERROR_ADDRESS        0x08
#define DFU_STATUS_ERROR_NOTDONE        0x09
#define DFU_STATUS_ERROR_FIRMWARE       0x0a
#define DFU_STATUS_ERROR_VENDOR         0x0b
#define DFU_STATUS_ERROR_USBR           0x0c
#define DFU_STATUS_ERROR_POR            0x0d
#define DFU_STATUS_ERROR_UNKNOWN        0x0e
#define DFU_STATUS_ERROR_STALLEDPKT     0x0f

/* DFU commands */
#define DFU_DETACH      0
#define DFU_DNLOAD      1
#define DFU_UPLOAD      2
#define DFU_GETSTATUS   3
#define DFU_CLRSTATUS   4
#define DFU_GETSTATE    5
#define DFU_ABORT       6

/* DFU interface */
#define DFU_IFF_DFU             0x0001  /* DFU Mode, (not Runtime) */

/* This is based off of DFU_GETSTATUS
 *
 *  1 unsigned byte bStatus
 *  3 unsigned byte bwPollTimeout
 *  1 unsigned byte bState
 *  1 unsigned byte iString
*/

struct dfu_status {
    unsigned char bStatus;
    unsigned int  bwPollTimeout;
    unsigned char bState;
    unsigned char iString;
};

struct dfu_if {
    struct usb_dfu_func_descriptor func_dfu;
    uint16_t quirks;
    uint16_t busnum;
    uint16_t devnum;
    uint16_t vendor;
    uint16_t product;
    uint16_t bcdDevice;
    uint8_t configuration;
    uint8_t interface;
    uint8_t altsetting;
    uint8_t flags;
    uint8_t bMaxPacketSize0;
    char *alt_name;
    char *serial_name;
    libusb_device *dev;
    libusb_device_handle *dev_handle;
    struct dfu_if *next;
};


int DFU_download(struct dfu_if *dif, const unsigned short length,unsigned char *data, unsigned short transaction);
int DFU_upload( libusb_device_handle *device,const unsigned short interface,const unsigned short length,const unsigned short transaction,unsigned char* data );
int DFU_get_status( struct dfu_if *dif, struct dfu_status *status );
int DFU_clear_status( libusb_device_handle *device,const unsigned short interface );
int DFU_download_data(struct dfu_if *dif, unsigned char *data, int size,int transaction);
int DFU_set_address(struct dfu_if *dif, unsigned int address);
int DFU_erase_page(struct dfu_if *dif, unsigned int address);
int DFU_download_end(struct dfu_if *dif,unsigned int address);


#endif /* DFU_PROTOCOL_H */
