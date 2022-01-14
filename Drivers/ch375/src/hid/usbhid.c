#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "bswap.h"
#include "ch375_usbhost.h"
#include "hid/usbhid.h"

// ms
#define TRANSFER_TIMEOUT 5000


// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03
 

int usbhid_read(USBHIDDevice *hiddev, uint8_t *buffer, int length, int *actual_len)
{
    USBDevice *udev;
    int ret;

    if (hiddev == NULL) {
        ERROR("param hiddev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (buffer == NULL) {
        ERROR("param buffer can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    udev = hiddev->udev;

    ret = ch375_host_interrupt_transfer(udev, hiddev->ep_in,
        buffer, length, actual_len, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("transfer(ep=0x%02X) failed, ret=%d", hiddev->ep_in, ret);
        if (ret == CH375_HST_ERRNO_DEV_DISCONNECT) {
            return USBHID_ERRNO_NO_DEV;
        }
        return USBHID_ERRNO_IO_ERROR;
    }
    return USBHID_ERRNO_SUCCESS;
}

// simple implementation
static int parser_hid_report(uint8_t *report, uint16_t len, uint8_t *hid_type)
{
    HIDItem item = {0};
    uint8_t *end = report + len;
    uint8_t *cur = report;
    
    // first item must be generic desktop
    cur = hid_fetch_item(cur, end, &item);
    if (cur == NULL) {
        ERROR("get item faild");
        return -1;
    }
    if (item.size != 1) {
        ERROR("first item size invalied");
        return -1;
    }
    if (!(item.format == HID_ITEM_FORMAT_SHORT &&
        item.type == HID_ITEM_TYPE_GLOBAL &&
        item.tag == HID_GLOBAL_ITEM_TAG_USAGE_PAGE &&
        (item.data.u8 << 16) == HID_UP_GENDESK)) {
        ERROR("first item is not generic desktop controls");
        return -1;
    }
    // Usage(Mouse/Keyboard)
    cur = hid_fetch_item(cur, end, &item);
    if (cur == NULL) {
        ERROR("get item faild");
        return -1;
    }
    if (item.size != 1) {
        ERROR("first item size invalied");
        return -1;
    }
    if (!(item.format == HID_ITEM_FORMAT_SHORT &&
        item.type == HID_ITEM_TYPE_LOCAL &&
        item.tag == HID_GLOBAL_ITEM_TAG_USAGE_PAGE)) {
        ERROR("Usage not found");
        return -1;
    }
    if (item.data.u8 == (HID_GD_MOUSE & 0xF)) {
        *hid_type = USBHID_TYPE_MOUSE;
        return 0;
    } else if (item.data.u8 == (HID_GD_KEYBOARD & 0xF)) {
        *hid_type = USBHID_TYPE_KEYBOARD;
        return 0;
    } else {
        ERROR("not support Usage(0x%02X)", item.data.u8);
        return -1;
    }

    return 0;
}

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

static int get_ep_in(USBDevice *udev, uint8_t interface_num, uint8_t *ep)
{
    assert(udev);
    assert(ep);
    assert(interface_num < udev->interface_cnt);
    USBInterface *interface = &udev->interface[interface_num];

    if (interface->endpoint_cnt < 1) {
        ERROR("interface_num(%d) have no endpoint", interface_num);
        return -1;
    }
    *ep = interface->endpoint[0].ep_num;
    return 0;
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
    uint8_t *raw_hid_report_desc = NULL;
    uint16_t raw_hid_report_desc_len = 0;
    uint8_t hid_type;
    uint8_t ep_in;
    int ret;
    int result;
    
    ret = get_hid_descriptor(udev, interface_num, &desc);
    if (ret < 0) {
        ERROR("can't find HID descriptor in udev(%04X:%04X) interface_num(%d)",
            udev->vid, udev->pid, interface_num);
        return USBHID_ERRNO_NOT_HID_DEV;
    }

    INFO("HID descriptor was founded, vesion=0x%04X, countryCode=0x%02X",
        le16_to_cpu(desc->bcdHID), desc->bCountryCode);

    if (desc->bNumDescriptors > 1) {
        // TODO not support
        ERROR("bNumDescriptors=%d is not support", desc->bNumDescriptors);
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    memset(hiddev, 0, sizeof(USBHIDDevice));

    set_idle(udev, interface_num, 0, 0);
    INFO("set idle done");
    
    raw_hid_report_desc_len = le16_to_cpu(desc->wClassDescriptorLength);
    raw_hid_report_desc = (uint8_t *)malloc(raw_hid_report_desc_len);
    if (raw_hid_report_desc == NULL) {
        ERROR("allocate raw HID report buffer(len=%d) failed", raw_hid_report_desc_len);
        return USBHID_ERRNO_ERROR;
    }
    memset(raw_hid_report_desc, 0, raw_hid_report_desc_len);

    // get data in endpoint
    ret = get_ep_in(udev, interface_num, &ep_in);
    if (ret < 0) {
        ERROR("get endpoint in(interface_num=%d) failed", interface_num);
        result = USBHID_ERRNO_NOT_SUPPORT;
        goto failed;
    }

    // get HID report
    ret = hid_get_class_descriptor(udev, interface_num, USB_DT_REPORT,
        raw_hid_report_desc, raw_hid_report_desc_len);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get HID REPORT(interface_num=%d, len=%d) failed, ret=%d",
            interface_num, raw_hid_report_desc_len, ret);
        result = USBHID_ERRNO_IO_ERROR;
        goto failed;
    }
    INFO("get HID REPORT (interface_num=%d, len=%d) done", interface_num, raw_hid_report_desc_len);

    // parser HID report
    ret = parser_hid_report(raw_hid_report_desc, raw_hid_report_desc_len, &hid_type);
    if (ret < 0) {
        ERROR("parser HID report failed, not support");
        result = USBHID_ERRNO_NOT_SUPPORT;
        goto failed;
    }

    hiddev->udev = udev;
    hiddev->interface_num = interface_num;
    hiddev->ep_in = ep_in;
    hiddev->raw_hid_report_desc = raw_hid_report_desc;
    hiddev->raw_hid_report_desc_len = raw_hid_report_desc_len;
    hiddev->hid_desc = desc;
    hiddev->hid_type = hid_type;

    return USBHID_ERRNO_SUCCESS;
failed:
    if (raw_hid_report_desc) {
        free(raw_hid_report_desc);
        raw_hid_report_desc = NULL;
    }
    return result;
}

void usbhid_close(USBHIDDevice *hiddev)
{
    if (hiddev == NULL) {
        return;
    }
    if (hiddev->raw_hid_report_desc) {
        free(hiddev->raw_hid_report_desc);
        hiddev->raw_hid_report_desc = NULL;
    }
    memset(hiddev, 0, sizeof(USBHIDDevice));
}