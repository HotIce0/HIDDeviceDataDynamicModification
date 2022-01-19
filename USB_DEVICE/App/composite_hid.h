#include <stdint.h>

#include "usbd_def.h"

#define COMPOSITE_HID_INTERFACE_NUM 2
#define COMPOSITE_HID_ENDPOINT_NUM 2

typedef struct USBDCompositeHID {
    uint8_t *config_desc;
    uint16_t config_desc_len;

    //interface
    uint8_t interface_cnt;
    uint8_t *hid_desc[COMPOSITE_HID_INTERFACE_NUM];

    uint8_t *report_desc[COMPOSITE_HID_INTERFACE_NUM];
    uint16_t report_desc_len[COMPOSITE_HID_INTERFACE_NUM];

    // endpoint
    uint8_t ep_addr[COMPOSITE_HID_ENDPOINT_NUM];
    uint8_t max_pack[COMPOSITE_HID_ENDPOINT_NUM];
    uint8_t interval[COMPOSITE_HID_ENDPOINT_NUM];
} USBDCompositeHID;


extern USBDCompositeHID usbd_composite_hid;

uint8_t USBD_COMPOSITE_HID_Init();
uint8_t USBD_COMPOSITE_HID_InterfaceRegister(uint8_t interface_num,
    uint8_t *hid_desc, uint8_t *report_descport, uint16_t length,
    uint8_t max_pack, uint8_t interval);