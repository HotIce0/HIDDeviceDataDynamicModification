#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include "hid.h"
#include "usbhid.h"


// Keyboard page Usage ID
// ref: 10 Keyboard/Keypad Page (0x07)
//      HIDUsageTables1.22.pdf

// LED
// Usage: bitmap offset
#define HID_LED_NUM_LOCK    0x01
#define HID_LED_CAPS_LOCK   0x02
#define HID_LED_SCROLL_LOCK 0x03

// Control Key
// Usage: bitmap offset = (HID_KBD_LEFT_CTRL & HID_KBD_CTRL_BM_OFF_MASK)
#define HID_KBD_CTRL_BM_OFF_MASK 0x0F
#define HID_KBD_LEFT_CTRL   0xE0
#define HID_KBD_LEFT_SHIFT  0xE1
#define HID_KBD_LEFT_ALT    0xE2
#define HID_KBD_LEFT_GUI    0xE3
#define HID_KBD_RIGHT_CTRL  0xE4
#define HID_KBD_RIGHT_SHIFT 0xE5
#define HID_KBD_RIGHT_ALT   0xE6
#define HID_KBD_RIGHT_GUI   0xE7

// Normal Key
// Usage: HID_KBD_LETTER('a'), Just support lowercase.
#define HID_KBD_LETTER(x) ((uint8_t)((x) - 0x5D))
// Usage: HID_KBD_NUMBER(1)
#define HID_KBD_NUMBER(x) ((x) == 0 ? 0x27: ((x) + 0x1D))
// Usage: HID_KBD_FX(2)
#define HID_KBD_FX(x) ((uint8_t)((x) + 0x39))

#define HID_KBD_ENTER      0x28
#define HID_KBD_SPACE      0x2C
#define HID_KBD_BACKSPACE  0x2A
// -
#define HID_KBD_MINUS      0x2D
// =
#define HID_KBD_EQUAL      0x2E


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
// int hid_keyboard_set_key(HIDKeyboard *dev, uint32_t key_code, uint32_t value, uint8_t is_last);

int hid_keyboard_fetch_report(HIDKeyboard *dev);
void hid_keyboard_close(HIDKeyboard *dev);
int hid_keyboard_open(USBHIDDevice *usbhid_dev, HIDKeyboard *dev);

#endif /* HID_KEYBOARD_H */