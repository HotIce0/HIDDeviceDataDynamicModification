#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include "hid.h"
#include "usbhid.h"

typedef struct HIDKeyboard {
    USBHIDDevice *hid_dev;
    
    HIDDataDescriptor control;
    HIDDataDescriptor led;
    HIDDataDescriptor keycode;

    uint32_t report_length;
} HIDKeyboard;

int hid_keyboard_get_ctrl(HIDKeyboard *dev, uint32_t ctrl_code, uint32_t *value, uint8_t is_last);
int hid_keyboard_set_ctrl(HIDKeyboard *dev, uint32_t ctrl_code, uint32_t value, uint8_t is_last);
int hid_keyboard_get_led(HIDKeyboard *dev, uint32_t led_code, uint32_t *value, uint8_t is_last);
int hid_keyboard_set_led(HIDKeyboard *dev, uint32_t led_code, uint32_t value, uint8_t is_last);
int hid_keyboard_get_key(HIDKeyboard *dev, uint32_t key_code, uint32_t *value, uint8_t is_last);
int hid_keyboard_set_key(HIDKeyboard *dev, uint32_t key_code, uint32_t value, uint8_t is_last);

int hid_keyboard_fetch_report(HIDKeyboard *dev);
void hid_keyboard_close(HIDKeyboard *dev);
int hid_keyboard_open(USBHIDDevice *usbhid_dev, HIDKeyboard *dev);

#endif /* HID_KEYBOARD_H */