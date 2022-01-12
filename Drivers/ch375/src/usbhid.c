#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "ch375_usbhost.h"
#include "usbhid.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define HID_TYPE_MOUSE    0
#define HID_TYPE_KEYBOARD 1

// ms
#define TRANSFER_TIMEOUT 5000


static int hid_get_class_descriptor(USBDevice *udev, uint8_t interface_num,
    uint8_t type, uint8_t *buf, uint16_t len)
{
    assert(udev);
    int actual_len;
    int ret;
    uint8_t retries = 4;

    do {
        ret = ch375_host_control_transfer(udev,
            USB_ENDPOINT_IN | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_INTERFACE,
            USB_REQUEST_GET_DESCRIPTOR,
            type << 8,
            interface_num,
            buf, len, &actual_len, TRANSFER_TIMEOUT);
        if (ret != CH375_HST_ERRNO_SUCCESS) {
            ERROR("send get class descriptor(type=0x%02X) failed, ret=%d", type, ret);    
        }
        retries--;
    } while (actual_len < len && retries);

    if (actual_len < len) {
        ERROR("no enough data, actaul_length=%d, length=%d", actual_len, len);
        return USBHID_ERRNO_ERROR;
    }
    return USBHID_ERRNO_SUCCESS;
}

static void set_idle(USBDevice *udev, uint8_t interface_num,
    uint8_t duration, uint8_t report_id)
{
    assert(udev);
    int ret;
    
    ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_OUT | USB_REQUEST_TYPE_CLASS | USB_RECIPIENT_INTERFACE,
        HID_SET_IDLE,
        (duration << 8) | report_id,
        interface_num,
        NULL, 0, NULL, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("set idle control(duration=0x%02X, report_id=0x%02X) transfer failed, ret=%d",
            duration, report_id, ret);
    }
}

static int get_hid_descriptor(USBDevice *udev, uint8_t interface_num, HIDDescriptor **hid_desc)
{
    USBDescriptor *desc = (USBDescriptor *)udev->raw_conf_desc;
    void *raw_conf_desc_end = (uint8_t *)udev->raw_conf_desc + udev->raw_conf_desc_len;
    uint8_t current_interface_num = 0;

    while ((void *)desc < raw_conf_desc_end) {
        // first run to skip configration descriptor.
        desc = (USBDescriptor *)((uint8_t *)desc + desc->bLength);

        switch (desc->bDesriptorType) {
            case USB_DT_INTERFACE: {
                current_interface_num = ((USBInterfaceDescriptor *)desc)->bInterfaceNumber;
                break;
            }
            case USB_DT_HID: {
                if (current_interface_num == interface_num) {
                    *hid_desc = (HIDDescriptor *)desc;
                    return 0;
                }
                break;
            }
            default: {
                break;
            }
        }
    }
    return -1;
}

int usbhid_open(USBDevice *udev, uint8_t interface_num, USBHIDDevice *hiddev)
{
    HIDDescriptor *desc = NULL;
    uint8_t *raw_hid_report = NULL;
    uint16_t raw_hid_report_len = 0;
    int ret;
    
    ret = get_hid_descriptor(udev, interface_num, &desc);
    if (ret < 0) {
        ERROR("can't find hid descriptor in udev(%04X:%04X) interface_num(%d)",
            udev->vid, udev->pid, interface_num);
        return USBHID_ERRNO_NOT_HID_DEV;
    }

    INFO("HID descriptor was founded, vesion=0x%04X, countryCode=0x%02X",
        ch375_le16_to_cpu(desc->bcdHID), desc->bCountryCode);

    if (desc->bNumDescriptors > 1) {
        // TODO not support
        ERROR("bNumDescriptors=%d is not support", desc->bNumDescriptors);
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    memset(hiddev, 0, sizeof(USBHIDDevice));

    set_idle(udev, interface_num, 0, 0);
    INFO("set idle done");
    
    raw_hid_report_len = ch375_le16_to_cpu(desc->wClassDescriptorLength);
    raw_hid_report = (uint8_t *)malloc(raw_hid_report_len);
    if (raw_hid_report == NULL) {
        ERROR("allocate raw hid report buffer(len=%d) failed", raw_hid_report_len);
        return USBHID_ERRNO_ERROR;
    }
    memset(raw_hid_report, 0, raw_hid_report_len);

    ret = hid_get_class_descriptor(udev, interface_num, USB_DT_REPORT,
        raw_hid_report, raw_hid_report_len);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get HID REPORT(interface_num=%d, len=%d) failed, ret=%d",
            interface_num, raw_hid_report_len, ret);
        return USBHID_ERRNO_IO_ERROR;
    }
    INFO("get HID REPORT(interface_num=%d, len=%d) done", interface_num, raw_hid_report_len);

    hiddev->hid_desc = desc;
    hiddev->udev = udev;
    hiddev->interface_num = interface_num;
    hiddev->raw_hid_report = raw_hid_report;
    hiddev->raw_hid_report_len = raw_hid_report_len;

    return USBHID_ERRNO_SUCCESS;
}