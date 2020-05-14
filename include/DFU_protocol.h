#ifndef DFU_PROTOCOL_H
#define DFU_PROTOCOL_H

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

enum dfu_state {
        DFU_STATE_appIDLE               = 0,
        DFU_STATE_appDETACH             = 1,
        DFU_STATE_dfuIDLE               = 2,
        DFU_STATE_dfuDNLOAD_SYNC        = 3,
        DFU_STATE_dfuDNBUSY             = 4,
        DFU_STATE_dfuDNLOAD_IDLE        = 5,
        DFU_STATE_dfuMANIFEST_SYNC      = 6,
        DFU_STATE_dfuMANIFEST           = 7,
        DFU_STATE_dfuMANIFEST_WAIT_RST  = 8,
        DFU_STATE_dfuUPLOAD_IDLE        = 9,
        DFU_STATE_dfuERROR              = 10
};

enum mode {
        MODE_NONE,
        MODE_VERSION,
        MODE_LIST,
        MODE_SUPPORTED_COMMANDS,
        MODE_DETACH,
        MODE_MASS_ERASE,
        MODE_UNPROTECT,
        MODE_READ_OPTIONS,
        MODE_UPLOAD,
        MODE_DOWNLOAD
};


/* DFU general commands */
#define DFU_SET_ADDRESS_CMD         0x21
#define DFU_SET_ADDRESS_CMD_LEN     5
#define DFU_ERASE_CMD               0x41
#define DFU_ERASE_CMD_LEN           5
#define DFU_MASS_ERASE_CMD          0x41
#define DFU_MASS_ERASE_CMD_LEN      1
#define DFU_EXIT_DFU_CMD            0x01
#define DFU_EXIT_DFU_CMD_LEN        0

struct dfu_status {
    unsigned char bStatus;
    unsigned int  bwPollTimeout;
    unsigned char bState;
    unsigned char iString;
};

#define VID 0x0483
#define PID 0xdf11

#define DFU_TIMEOUT 15000

#define     BIN_DATA_MAX_SIZE   131072
#define     ERASE_PAGE_SIZE     2048
#define     WRITE_PAGE_SIZE     1024
#define     ADDRESS 0x08000000

int DFU_download(libusb_device_handle *handle, const unsigned short wLength,unsigned char *data, unsigned short wValue);
int DFU_upload( libusb_device_handle *handle, const unsigned short wLength,const unsigned short wValue,unsigned char* data );
int DFU_get_status( libusb_device_handle *handle, struct dfu_status *status );
int DFU_get_state( libusb_device_handle *handle );
int DFU_clear_status( libusb_device_handle *handle );
int DFU_download_data(libusb_device_handle *handle, unsigned char *data, int size,int transaction);
int DFU_set_address(libusb_device_handle *handle, unsigned int address);
int DFU_erase_page(libusb_device_handle *handle, unsigned int address);
int DFU_download_end(libusb_device_handle *handle, unsigned int address);
int DFU_mass_erase(libusb_device_handle *handle);
int DFU_go_to_idle(libusb_device_handle *handle);
int DFU_general_command(libusb_device_handle *handle, unsigned int address , unsigned char command, unsigned int cmdlen);

#endif /* DFU_PROTOCOL_H */
