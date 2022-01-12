#include "hid.h"

enum USBHID_ERRNO{
    USBHID_ERRNO_SUCCESS = 0,
    USBHID_ERRNO_ERROR = -1,
    USBHID_ERRNO_PARAM_INVALID = -2,
    USBHID_ERRNO_IO_ERROR = -3,
    USBHID_ERRNO_TIMEOUT = -6,
	USBHID_ERRNO_DEV_DISCONNECT = -7,
    USBHID_ERRNO_NOT_HID_DEV = -8,
    USBHID_ERRNO_NOT_SUPPORT = -9,
};

typedef struct USBHIDDevice {
    USBDevice *udev;
    uint8_t interface_num;

    uint8_t *raw_hid_report;
    uint16_t raw_hid_report_len;

    HIDDescriptor *hid_desc;

    uint8_t hid_type;
} USBHIDDevice;

int usbhid_open(USBDevice *udev, uint8_t interface_num, USBHIDDevice *hiddev);