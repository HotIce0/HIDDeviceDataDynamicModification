#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "hid/hid_keyboard.h"

int hid_keyboard_get_ctrl(HIDKeyboard *dev, uint32_t ctrl_code, uint32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = 0;
    uint8_t bit_off = ctrl_code & HID_KBD_CTRL_BM_OFF_MASK;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    desc = &dev->control;
    if (ctrl_code > desc->physical_maximum ||
        ctrl_code < desc->physical_minimum) {
        ERROR("param ctrl_code is invalied, (0x%02X) out of range [0x%02X, 0x%02X]",
            ctrl_code, desc->physical_minimum, desc->physical_maximum);
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }
    field_buf = report_buf + desc->report_buf_off;

    *value = (field_buf[byte_off] & (1 << bit_off)) ? 1: 0;
    return USBHID_ERRNO_SUCCESS;
}

int hid_keyboard_set_ctrl(HIDKeyboard *dev, uint32_t ctrl_code, uint32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = 0;
    uint8_t bit_off = ctrl_code & HID_KBD_CTRL_BM_OFF_MASK;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    desc = &dev->control;
    if (ctrl_code > desc->physical_maximum ||
        ctrl_code < desc->physical_minimum) {
        ERROR("param ctrl_code is invalied, (0x%02X) out of range [0x%02X, 0x%02X]",
            ctrl_code, desc->physical_minimum, desc->physical_maximum);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }
    field_buf = report_buf + desc->report_buf_off;

    field_buf[byte_off] = field_buf[byte_off] | (1 << bit_off);
    return USBHID_ERRNO_SUCCESS;
}

int hid_keyboard_get_led(HIDKeyboard *dev, uint32_t led_code, uint32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = 0;
    uint8_t bit_off = led_code;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    desc = &dev->led;
    if (led_code > desc->physical_maximum ||
        led_code < desc->physical_minimum) {
        ERROR("param led_code is invalied, (0x%02X) out of range [0x%02X, 0x%02X]",
            led_code, desc->physical_minimum, desc->physical_maximum);
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (value == NULL) {
        ERROR("param value can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }
    field_buf = report_buf + desc->report_buf_off;

    *value = (field_buf[byte_off] & (1 << bit_off)) ? 1: 0;
    return USBHID_ERRNO_SUCCESS;
}

int hid_keyboard_set_led(HIDKeyboard *dev, uint32_t led_code, uint32_t value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    uint8_t byte_off = 0;
    uint8_t bit_off = led_code;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    desc = &dev->led;
    if (led_code > desc->physical_maximum ||
        led_code < desc->physical_minimum) {
        ERROR("param ctrl_code is invalied, (0x%02X) out of range [0x%02X, 0x%02X]",
            led_code, desc->physical_minimum, desc->physical_maximum);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }
    field_buf = report_buf + desc->report_buf_off;

    field_buf[byte_off] = field_buf[byte_off] | (1 << bit_off);
    return USBHID_ERRNO_SUCCESS;
}

int hid_keyboard_get_key(HIDKeyboard *dev, uint32_t key_code, uint32_t *value, uint8_t is_last)
{
    HIDDataDescriptor *desc;
    uint8_t *report_buf;
    uint8_t *field_buf;
    int i;
    int ret;

    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    desc = &dev->keycode;
    if (key_code > desc->physical_maximum ||
        key_code < desc->physical_minimum) {
        ERROR("param ctrl_code is invalied, (0x%02X) out of range [0x%02X, 0x%02X]",
            key_code, desc->physical_minimum, desc->physical_maximum);
        return USBHID_ERRNO_PARAM_INVALID;
    }

    ret = usbhid_get_report_buffer(dev->hid_dev, &report_buf, NULL, is_last);
    if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("get report buffer failed, ret=%d", ret);
        return ret;
    }
    field_buf = report_buf + desc->report_buf_off;
    // INFO("dump keybuf");
    // hex_dump(field_buf, 6);
    for (i = 0; i < desc->count; i++) {
        if (field_buf[i] == key_code) {
            *value = 1;
            return USBHID_ERRNO_SUCCESS;
        }
    }
    *value = 0;
    return USBHID_ERRNO_SUCCESS;
}

// int hid_keyboard_set_key(HIDKeyboard *dev, uint32_t key_code, uint32_t value, uint8_t is_last)
// {
//     return USBHID_ERRNO_SUCCESS;
// }

int hid_keyboard_fetch_report(HIDKeyboard *dev)
{
    if (dev == NULL) {
        ERROR("param dev can't be NULL");
        return USBHID_ERRNO_PARAM_INVALID;
    }

    return usbhid_fetch_report(dev->hid_dev);
}

void hid_keyboard_close(HIDKeyboard *dev)
{
    if (dev == NULL) {
        return;
    }
    usbhid_free_report_buffer(dev->hid_dev);
    memset(dev, 0, sizeof(HIDKeyboard));
}

static int parser_hid_report(HIDKeyboard *dev, uint8_t *report, uint16_t len)
{
    HIDDataDescriptor *ctrl = &dev->control;
    HIDDataDescriptor *led = &dev->led;
    HIDDataDescriptor *keycode = &dev->keycode;
    
    // PVID=0951:16D2 HyperX Alloy FPS Pro Mechanical Gaming Keyboard
    // {00}  {00}  {05 00 00 00 00 00}
    // CTRL  LED   KEYCODE(6 key)
    dev->report_length = 8;

    // control button
    ctrl->physical_minimum = 0xe0;
    ctrl->physical_maximum = 0xe7;
    ctrl->logical_minimum = 0;
    ctrl->logical_maximum = 1;
    ctrl->size = 1;
    ctrl->count = 8;
    ctrl->report_buf_off = 0;

    // led
    led->physical_minimum = 0x01;
    led->physical_maximum = 0x05;
    led->logical_minimum = 0;
    led->logical_maximum = 1;
    led->size = 8; // led 5 bits + padding 3 bits
    led->count = 1;
    led->report_buf_off = 1;

    // keycode
    keycode->physical_minimum = 0x00;
    keycode->physical_maximum = 0xFF;
    keycode->logical_minimum = 0;
    keycode->logical_maximum = 255;
    keycode->size = 8;
    keycode->count = 6;
    keycode->report_buf_off = 2;

    return 0;
}

int hid_keyboard_open(USBHIDDevice *usbhid_dev, HIDKeyboard *dev)
{
    int ret;

    if (dev == NULL || usbhid_dev == NULL) {
        ERROR("param %s can't be NULL", dev == NULL ? "dev": "usbhid_dev");
        return USBHID_ERRNO_PARAM_INVALID;
    }
    if (usbhid_dev->hid_type != USBHID_TYPE_KEYBOARD) {
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    memset(dev, 0, sizeof(HIDKeyboard));
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