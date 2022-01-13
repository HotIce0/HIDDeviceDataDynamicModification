#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "hid/hid_mouse.h"
#include "hid/hid.h"

static uint8_t *find_item(uint8_t *start, uint8_t *end,
    uint8_t type, uint8_t tag, uint8_t size, uint32_t data)
{
    HIDItem item = {0};
    uint8_t *cur = start;
    uint8_t *next;
    while (cur < end) {
        next = hid_fetch_item(cur++, end, &item);
        if (next == NULL) {
            ERROR("get item faild");
            return NULL;
        }
        if (item.size == size &&
            item.type == type &&
            item.tag == tag &&
            item.data.u8 == data) {
            switch (size) {
                case 0:
                    goto done;
                    break;
                case 1:
                    if (item.data.u8 == data) {
                        goto done;
                    }
                    break;
                case 2:
                    if (item.data.u16 == data) {
                        goto done;
                    }
                    break;
                case 3:
                    if (item.data.u32 == data) {
                        goto done;
                    }
                    break;
            }
        }
        if ((end - cur) < item.size) {
            return NULL;
        }
        cur = next;
    }
    return NULL;
done:
    return cur + item.size;
}

static int parser_hid_report(uint8_t *report, uint16_t len, HIDMouse *mouse)
{
    uint8_t *end = report + len;
    uint8_t *collect_app;
    uint8_t *collect_physical;
    uint8_t *usage_page_btn;
    uint8_t *usage_page_gd_ctrl;

    // Collection Application
    collect_app = find_item(report, end, HID_ITEM_TYPE_MAIN, HID_MAIN_ITEM_TAG_BEGIN_COLLECTION,
        1, HID_COLLECTION_APPLICATION);
    if (collect_app == NULL) {
        ERROR("find collection(application) failed");
        return -1;
    }
    // Usage Pointer
    if (find_item(collect_app, end, HID_ITEM_TYPE_LOCAL, HID_LOCAL_ITEM_TAG_USAGE,
        1, HID_GD_POINTER & 0xF) == NULL) {
        ERROR("find usage pointer item failed");
        return -1;
    }
    // Collection Physical
    collect_physical = find_item(collect_app, end, HID_ITEM_TYPE_MAIN, HID_MAIN_ITEM_TAG_BEGIN_COLLECTION,
        1, HID_COLLECTION_PHYSICAL);
    if (collect_physical == NULL) {
        ERROR("find collection(physical) failed");
        return -1;
    }
    // Usage Page Button
    usage_page_btn = find_item(collect_physical, end, HID_ITEM_TYPE_GLOBAL, HID_GLOBAL_ITEM_TAG_USAGE_PAGE,
        1, HID_UP_BUTTON >> 16);
    if (usage_page_btn == NULL) {
        ERROR("find Usage Page(Button) failed");
        return -1;
    }
    // TODO
    
    // Usage Page Generic Desktop Controls
    usage_page_gd_ctrl = find_item(collect_physical, end, HID_ITEM_TYPE_GLOBAL, HID_GLOBAL_ITEM_TAG_USAGE_PAGE,
        1, HID_UP_BUTTON >> 16);
    if (usage_page_gd_ctrl == NULL) {
        ERROR("find Usage Page(Generic Desktop Controls) failed");
        return -1;
    }
    // TODO

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

    ret = parser_hid_report(usbhid_dev->raw_hid_report,
        usbhid_dev->raw_hid_report_len,
        mouse);
    if (ret < 0) {
        ERROR("parser hid report failed, not support");
        return USBHID_ERRNO_NOT_SUPPORT;
    }

    return USBHID_ERRNO_SUCCESS;
}