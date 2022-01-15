#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "bswap.h"
#include "hid/hid_mouse.h"
#include "hid/hid.h"


static inline uint8_t *get_report_buffer(HIDMouse *mouse, uint8_t is_last)
{
    assert(mouse);

    if (is_last) {
        return mouse->report_buffer + mouse->report_buffer_last_offset;
    } else {
        uint8_t offset;
        offset = mouse->report_buffer_last_offset ? 0: mouse->report_buffer_last_offset;
        return mouse->report_buffer + offset;
    }
}

int hid_mouse_get_report_buffer(HIDMouse *mouse, uint8_t **buffer, uint8_t is_last)
{
    if (mouse == NULL) {
        ERROR("param mouse can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (buffer == NULL) {
        ERROR("param buffer can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    *buffer = get_report_buffer(mouse, is_last);
    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_get_button(HIDMouse *mouse, uint32_t button_num, uint32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = button_num / 8;
    uint8_t bit_off = button_num % 8;

    if (mouse == NULL) {
        ERROR("param mouse can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (button_num >= mouse->button.count) {
        ERROR("param buttom_num(%u) is invalied", button_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    desc = &mouse->button;
    report_buf = get_report_buffer(mouse, is_last);

    field_buf = report_buf + desc->report_buf_off;
    // INFO("button_num=%d, byte_off=%d, bit_off=%d", button_num, byte_off, bit_off);
    *value = field_buf[byte_off] & (0x01 << bit_off) ? 1: 0;
    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_set_button(HIDMouse *mouse, uint32_t button_num, uint32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = button_num / 8;
    uint8_t bit_off = button_num % 8;

    if (mouse == NULL) {
        ERROR("param mouse can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (button_num >= mouse->button.count) {
        ERROR("param buttom_num(%u) is invalied", button_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    desc = &mouse->button;
    report_buf = get_report_buffer(mouse, is_last);
    field_buf = report_buf + desc->report_buf_off;

    if (value) {
        field_buf[byte_off] = field_buf[byte_off] | (0x01 << bit_off);
    } else {
        field_buf[byte_off] = field_buf[byte_off] & ~(0x01 << bit_off);
    }
    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_get_orientation(HIDMouse *mouse, uint32_t axis_num, int32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t value_byte_size;

    if (mouse == NULL) {
        ERROR("param mouse can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (axis_num >= mouse->orientation.count) {
        ERROR("param axis_num(%u) is invalid", axis_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    desc = &mouse->orientation;
    report_buf = get_report_buffer(mouse, is_last);
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
    DEBUG("##GET BTN:%02X %02X X:%02X %02X Y:%02X %02X WHEEL:%02X UNKOWN:%02X",
        report_buf[0],
        report_buf[1],
        report_buf[2],
        report_buf[3],
        report_buf[4],
        report_buf[5],
        report_buf[6],
        report_buf[7]);

    return USBHID_ERRNO_SUCCESS;
}

int hid_mouse_set_orientation(HIDMouse *mouse, uint32_t axis_num, int32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t value_byte_size;

    if (mouse == NULL) {
        ERROR("param mouse can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (axis_num >= mouse->orientation.count) {
        ERROR("param axis_num(%u) is invalid", axis_num);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    desc = &mouse->orientation;
    report_buf = get_report_buffer(mouse, is_last);
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

int hid_mouse_fetch_report(HIDMouse *mouse)
{
    USBHIDDevice *hiddev = mouse->hid_dev;
    uint8_t *last_report_buffer;
    int actual_len;
    int ret;

    last_report_buffer = get_report_buffer(mouse, 0);
    ret = usbhid_read(hiddev, last_report_buffer,
        mouse->report_length, &actual_len);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("fetch report failed, ret=%d", ret);
        return ret;
    }
    // DEBUG: dump report
    DEBUG("BTN:%02X %02X X:%02X %02X Y:%02X %02X WHEEL:%02X UNKOWN:%02X",
        last_report_buffer[0],
        last_report_buffer[1],
        last_report_buffer[2],
        last_report_buffer[3],
        last_report_buffer[4],
        last_report_buffer[5],
        last_report_buffer[6],
        last_report_buffer[7]);
    // switch buffer
    if (mouse->report_buffer_last_offset) {
        mouse->report_buffer_last_offset = 0;
    } else {
        mouse->report_buffer_last_offset = mouse->report_length;
    }
    DEBUG("mouse fetch report success");
    return USBHID_ERRNO_SUCCESS;
}

void hid_mouse_close(HIDMouse *mouse)
{
    if (mouse == NULL) {
        return;
    }
    if (mouse->report_buffer) {
        free(mouse->report_buffer);
        mouse->report_buffer = NULL;
    }
    memset(mouse, 0, sizeof(HIDMouse));
}

static int parser_hid_report(HIDMouse *mouse, uint8_t *report, uint16_t len)
{
    HIDDataDescriptor *btn = &mouse->button;
    HIDDataDescriptor *orien = &mouse->orientation;
    
    // PVID=046D:C092 G102 LIGHTSYNC Gaming Mouse
    mouse->report_length = 8;

    btn->physical_minimum = 1;
    btn->physical_maximum = 16;
    btn->logical_minimum = 0;
    btn->logical_maximum = 1;
    btn->size = 1;
    btn->count = 16;
    btn->report_buf_off = 0;

    orien->logical_minimum = -32767;
    orien->logical_maximum = 32767;
    orien->size = 16;
    orien->count = 2;
    orien->report_buf_off = 2;

    return 0;
}

int hid_mouse_open(USBHIDDevice *usbhid_dev, HIDMouse *mouse)
{
    int ret;

    if (mouse == NULL || usbhid_dev == NULL) {
        ERROR("param %s can't be NULL", mouse == NULL ? "mouse": "usbhid_dev");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (usbhid_dev->hid_type != USBHID_TYPE_MOUSE) {
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    memset(mouse, 0, sizeof(HIDMouse));
    mouse->hid_dev = usbhid_dev;

    ret = parser_hid_report(mouse,
        usbhid_dev->raw_hid_report_desc,
        usbhid_dev->raw_hid_report_desc_len);
    if (ret < 0) {
        ERROR("parser hid report failed, not support");
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    mouse->report_buffer_length = mouse->report_length * 2;
    mouse->report_buffer = (uint8_t *)malloc(mouse->report_buffer_length);
    if (mouse->report_buffer == NULL) {
        ERROR("allocate report buffer failed");
        return USBHID_ERRNO_ERROR;
    }
    memset(mouse->report_buffer, 0, mouse->report_buffer_length);

    return USBHID_ERRNO_SUCCESS;
}