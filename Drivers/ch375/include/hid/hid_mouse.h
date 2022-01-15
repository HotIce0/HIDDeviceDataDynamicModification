#ifndef HID_MOUSE_H
#define HID_MOUSE_H
#include "hid.h"
#include "usbhid.h"

#define HID_MOUSE_BUTTON_LEFT    0
#define HID_MOUSE_BUTTON_RIGHT   1
#define HID_MOUSE_BUTTON_MIDDLE  2
#define HID_MOUSE_BUTTON_EX_BASE 3

#define HID_MOUSE_AXIS_X 0
#define HID_MOUSE_AXIS_Y 1

typedef struct HIDMouse {
    USBHIDDevice *hid_dev;

    HIDDataDescriptor button;
    HIDDataDescriptor orientation;
    uint32_t report_length;
} HIDMouse;

int hid_mouse_get_button(HIDMouse *dev, uint32_t button_num, uint32_t *value, uint8_t is_last);
int hid_mouse_set_button(HIDMouse *dev, uint32_t button_num, uint32_t value, uint8_t is_last);
int hid_mouse_get_orientation(HIDMouse *dev, uint32_t axis_num, int32_t *value, uint8_t is_last);
int hid_mouse_set_orientation(HIDMouse *dev, uint32_t axis_num, int32_t value, uint8_t is_last);

int hid_mouse_fetch_report(HIDMouse *dev);
void hid_mouse_close(HIDMouse *dev);
int hid_mouse_open(USBHIDDevice *usbhid_dev, HIDMouse *dev);

#endif /* HID_MOUSE_H */