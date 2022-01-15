#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "bswap.h"
#include "hid/hid_mouse.h"
#include "hid/hid.h"


int hid_mouse_get_button(HIDMouse *dev, uint32_t button_num, uint32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = button_num / 8;
    uint8_t bit_off = button_num % 8;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (button_num >= dev->button.count) {
        ERROR("param buttom_num(%u) is invalied", button_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }

    desc = &dev->button;
    field_buf = report_buf + desc->report_buf_off;
    // INFO("button_num=%d, byte_off=%d, bit_off=%d", button_num, byte_off, bit_off);
    *value = field_buf[byte_off] & (0x01 << bit_off) ? 1: 0;
    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_set_button(HIDMouse *dev, uint32_t button_num, uint32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = button_num / 8;
    uint8_t bit_off = button_num % 8;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (button_num >= dev->button.count) {
        ERROR("param buttom_num(%u) is invalied", button_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }

    desc = &dev->button;
    field_buf = report_buf + desc->report_buf_off;

    if (value) {
        field_buf[byte_off] = field_buf[byte_off] | (0x01 << bit_off);
    } else {
        field_buf[byte_off] = field_buf[byte_off] & ~(0x01 << bit_off);
    }
    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_get_orientation(HIDMouse *dev, uint32_t axis_num, int32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t value_byte_size;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (axis_num >= dev->orientation.count) {
        ERROR("param axis_num(%u) is invalid", axis_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }

    desc = &dev->orientation;
    field_buf = report_buf + desc->report_buf_off;
    value_byte_size = (desc->size / 8);

    switch (value_byte_size) {
        case 1:
            *value = ((int8_t *)field_buf)[axis_num];
            break;
        case 2:
            *value = le16_to_cpu(((int16_t *)field_buf)[axis_num]);
            break;
        case 4:
            *value = le32_to_cpu(((int32_t *)field_buf)[axis_num]);
            break;
        default:
            ERROR("not expect");
            assert(0);
            break;
    }

    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_set_orientation(HIDMouse *dev, uint32_t axis_num, int32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t value_byte_size;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (axis_num >= dev->orientation.count) {
        ERROR("param axis_num(%u) is invalid", axis_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }

    desc = &dev->orientation;
    field_buf = report_buf + desc->report_buf_off;
    value_byte_size = (desc->size / 8);

    switch (value_byte_size) {
        case 1:
            ((int8_t *)field_buf)[axis_num] = (int8_t)value;
            break;
        case 2:
            ((int16_t *)field_buf)[axis_num] = cpu_to_le16((int16_t)value);
            break;
        case 4:
            ((int32_t *)field_buf)[axis_num] = cpu_to_le32((int32_t)value);
            break;
        default:
            ERROR("not expect");
            assert(0);
            break;
    }

    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_fetch_report(HIDMouse *dev)
{
    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }

    return usbhid_fetch_report(dev->hid_dev);
}

void hid_mouse_close(HIDMouse *dev)
{
    if (dev == NULL) {
        return;
    }
    usbhid_free_report_buffer(dev->hid_dev);
    memset(dev, 0, sizeof(HIDMouse));
}

static int parser_hid_report(HIDMouse *dev, uint8_t *report, uint16_t len)
{
    HIDDataDescriptor *btn = &dev->button;
    HIDDataDescriptor *orien = &dev->orientation;
    
    // PVID=046D:C092 G102 LIGHTSYNC Gaming Mouse
    dev->report_length = 8;

    btn->physical_minimum = 1;
    btn->physical_maximum = 16;
    btn->logical_minimum = 0;
    btn->logical_maximum = 1;
    btn->size = 1;
    btn->count = 16;
    btn->report_buf_off = 0;

    orien->physical_minimum = 1;
    orien->physical_maximum = 2;
    orien->logical_minimum = -32767;
    orien->logical_maximum = 32767;
    orien->size = 16;
    orien->count = 2;
    orien->report_buf_off = 2;

    return 0;
}

int hid_mouse_open(USBHIDDevice *usbhid_dev, HIDMouse *dev)
{
    int ret;

    if (dev == NULL || usbhid_dev == NULL) {
        ERROR("param %s can't be NULL", dev == NULL ? "dev": "usbhid_dev");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (usbhid_dev->hid_type != USBHID_TYPE_MOUSE) {
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    memset(dev, 0, sizeof(HIDMouse));
    dev->hid_dev = usbhid_dev;

    ret = parser_hid_report(dev,
        usbhid_dev->raw_hid_report_desc,
        usbhid_dev->raw_hid_report_desc_len);
    if (ret < 0) {
        ERROR("parser hid report failed, not support");
        return USBHID_ERRNO_NOT_SUPPORT;
    }
    assert(dev->report_length != 0);

    ret = usbhid_alloc_report_buffer(usbhid_dev, dev->report_length);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("allocate report buffer(length=%d) failed",
            dev->report_length);
        return USBHID_ERRNO_ALLOC_FAILED;
    }

    return USBHID_ERRNO_SUCCESS;
}