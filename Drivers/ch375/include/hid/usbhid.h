#ifndef USBHID_H
#define USBHID_H

#include "hid.h"
#include "ch375_usbhost.h"

#define USBHID_TYPE_MOUSE    0
#define USBHID_TYPE_KEYBOARD 1

enum USBHID_ERRNO{
    USBHID_ERRNO_SUCCESS = 0,
    USBHID_ERRNO_ERROR = -1,
    USBHID_ERRNO_PARAM_INVALID = -2,
    USBHID_ERRNO_IO_ERROR = -3,
    USBHID_ERRNO_NO_DEV = -4,
    USBHID_ERRNO_TIMEOUT = -6,
	USBHID_ERRNO_DEV_DISCONNECT = -7,
    USBHID_ERRNO_NOT_HID_DEV = -8,
    USBHID_ERRNO_NOT_SUPPORT = -9,
};

#pragma pack (push)
#pragma pack (1)

typedef struct HIDDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdHID;
	uint8_t bCountryCode;
	uint8_t bNumDescriptors;
	uint8_t bClassDescriptorType;
	uint16_t wClassDescriptorLength;
} HIDDescriptor;

#pragma pack (pop)

typedef struct USBHIDDevice {
    USBDevice *udev;
    uint8_t interface_num;
    uint8_t ep_in;

    uint8_t *raw_hid_report_desc;
    uint16_t raw_hid_report_desc_len;

    HIDDescriptor *hid_desc;

    uint8_t hid_type;
} USBHIDDevice;

int usbhid_read(USBHIDDevice *hiddev, uint8_t *buffer, int length, int *actual_len);
void usbhid_close(USBHIDDevice *hiddev);
int usbhid_open(USBDevice *udev, uint8_t interface_num, USBHIDDevice *hiddev);

#endif /* USBHID_H */