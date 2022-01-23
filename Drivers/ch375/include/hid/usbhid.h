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
    USBHID_ERRNO_ALLOC_FAILED = -5,
    USBHID_ERRNO_TIMEOUT = -6,
	USBHID_ERRNO_DEV_DISCONNECT = -7,
    USBHID_ERRNO_NOT_HID_DEV = -8,
    USBHID_ERRNO_NOT_SUPPORT = -9,
    USBHID_ERRNO_BUFFER_NOT_ALLOC = -10,
};

#define USBHID_NOW  0
#define USBHID_LAST 1

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
    HIDDescriptor *hid_desc;

    uint8_t *raw_hid_report_desc;
    uint16_t raw_hid_report_desc_len;
    uint8_t hid_type;

    // ### private (do not use it)
    // initialze by hid_XX(mouse, keyboard)
    uint32_t report_length;

    uint8_t *report_buffer; // [current report, last report]
    uint32_t report_buffer_length; // report_buffer_length = 2 * report_length
    uint32_t report_buffer_last_offset;
} USBHIDDevice;

int usbhid_fetch_report(USBHIDDevice *dev);

int usbhid_alloc_report_buffer(USBHIDDevice *dev, uint32_t length);
void usbhid_free_report_buffer(USBHIDDevice *dev);
int usbhid_get_report_buffer(USBHIDDevice *dev, uint8_t **buffer, uint32_t *length, uint8_t is_last);

void usbhid_close(USBHIDDevice *dev);
int usbhid_open(USBDevice *udev, uint8_t interface_num, USBHIDDevice *dev);

#endif /* USBHID_H */