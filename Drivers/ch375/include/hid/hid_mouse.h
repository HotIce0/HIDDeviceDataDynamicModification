#ifndef HID_MOUSE_H
#define HID_MOUSE_H

#include "usbhid.h"


typedef struct HIDMouse {
    USBHIDDevice *hid_dev;

    uint8_t btn_bits_cnt;
    uint8_t btn_filled_bits_cnt;

    uint32_t move_value_min;
    uint32_t move_value_max;
    uint8_t move_bits_cnt; // per direction bits count

    uint16_t wheel;
    uint32_t wheel_value_min;
    uint32_t wheel_value_max;
    uint8_t wheel_bits_cnt;
} HIDMouse;

int hid_mouse_open(USBHIDDevice *usbhid_dev, HIDMouse *mouse);

#endif /* HID_MOUSE_H */