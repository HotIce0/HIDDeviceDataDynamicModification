#include <assert.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "usb.h"
#include "ch375.h"
#include "ch375_usbhost.h"


#define SETUP_IN(x) ((x) & 0x80)

#define RESET_WAIT_DEVICE_RECONNECT_TIMEOUT_MS 1000
#define TRANSFER_TIMEOUT 5000

#define TRANSFER_BUFLEN 64

inline static void fill_control_setup(uint8_t *buf, 
    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
    assert(buf);
    USBControlSetup *cs = (USBControlSetup *)buf;
    cs->bmRequestType = request_type;
    cs->bRequest = bRequest;
    cs->wValue = ch375_cpu_to_le16(wValue);
    cs->wIndex = ch375_cpu_to_le16(wIndex);
    cs->wLength = ch375_cpu_to_le16(wLength);
}

// static int get_ep(USBDevice *udev)
// {
// }

int ch375_host_control_transfer(USBDevice *udev,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	uint8_t *data, uint16_t wLength, int *actual_length, uint32_t timeout)
{
    CH375Context *ctx;
    int ret;
    uint8_t setup_buf[CONTROL_SETUP_SIZE];
    int residue_len = wLength;
    uint8_t tog = 0;
    int offset = 0;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (udev->context == NULL) {
        ERROR("param udev->context can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    ctx = udev->context;

    ret = ch375_set_retry(ctx, CH375_SET_RETRY_TIMES_INFINITY);
    if (ret != CH375_SUCCESS) {
        ERROR("set ch375 retry infinity failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    
    // SETUP
    DEBUG("send SETUP");
    fill_control_setup(setup_buf, request_type, bRequest, wValue, wIndex, wLength);
    // hex_dump(setup_buf, CONTROL_SETUP_SIZE); //TODO
    ret = ch375_write_block_data(ctx, setup_buf, CONTROL_SETUP_SIZE);
    if (ret != CH375_SUCCESS) {
        ERROR("write control setup packet failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }

    ret = ch375_send_token(ctx, 0, tog, USB_PID_SETUP);
    if (ret != CH375_SUCCESS) {
        ERROR("send token(SETUP) failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    DEBUG("send token(SETUP) done");

    // DATA0
    while (residue_len) {
        uint8_t len = residue_len > udev->ep0_maxpack ? udev->ep0_maxpack: residue_len;
        uint8_t actual_len;
        tog = tog^1;
        if (SETUP_IN(request_type)) {
            // IN
            ret = ch375_send_token(ctx, 0, tog, USB_PID_IN);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(IN) failed, ret=%d", ret);
                return CH375_HST_ERRNO_ERROR;
            }
            ret = ch375_read_block_data(ctx, data + offset, len, &actual_len);
            if (ret != CH375_SUCCESS) {
                ERROR("read control in data failed, (residue_len=%d, len=%d, actual_len=%d), ret=%d",
                    residue_len, len, actual_len, ret);
                return CH375_HST_ERRNO_ERROR;
            }
            DEBUG("test, residue_len=%d, offset=%d", actual_len, residue_len, offset);
            residue_len -= actual_len;
            offset += actual_len;
            DEBUG("test, actual_len=%d, residue_len=%d, offset=%d", actual_len, residue_len, offset);
            if (actual_len < udev->ep0_maxpack) {
                // short packet, done
                break;
            }
        } else {
            // OUT
            ret = ch375_write_block_data(ctx, data + offset, len);
            if (ret != CH375_SUCCESS) {
                ERROR("write control OUT data failed, (residue_len=%d, len=%d, actual_len=%d), ret=%d",
                    residue_len, len, actual_len, ret);
                return CH375_HST_ERRNO_ERROR;
            }
            ret = ch375_send_token(ctx, 0, tog, USB_PID_OUT);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(OUT) failed, ret=%d", ret);
                return CH375_HST_ERRNO_ERROR;
            }
            residue_len -= actual_len;
            offset += actual_len;
        }
    }
    // ACK
    tog = 1;
    if (SETUP_IN(request_type)) {
        ret = ch375_write_block_data(ctx, data + offset, 0);
        if (ret != CH375_SUCCESS) {
            ERROR("write control OUT data failed, len=0, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
        ret = ch375_send_token(ctx, 0, tog, USB_PID_OUT);
        if (ret != CH375_SUCCESS) {
            ERROR("send token(OUT) failed, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
    } else {
        ret = ch375_send_token(ctx, 0, tog, USB_PID_IN);
        if (ret != CH375_SUCCESS) {
            ERROR("send token(IN) failed, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
    }
    *actual_length = offset;
    return CH375_HST_ERRNO_SUCCESS;
}

static int get_device_descriptor(USBDevice *udev, uint8_t *buf)
{
    assert(buf);
    int actual_len = 0;
    int ret;
    
    ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_IN | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE,
        USB_REQUEST_GET_DESCRIPTOR,
        USB_DT_DEVICE << 8, 0,
        buf, sizeof(USBDeviceDescriptor), &actual_len, TRANSFER_TIMEOUT);

    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("control transfer failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    if (actual_len < sizeof(USBDeviceDescriptor)) {
        ERROR("no enough data");
        return CH375_HST_ERRNO_ERROR;
    }
    return CH375_HST_ERRNO_SUCCESS;
}

int ch375_host_reset_dev(USBDevice *udev)
{
    int ret;
    uint8_t conn_status;
    uint8_t speed;
    CH375Context *ctx;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (udev->context == NULL) {
        ERROR("param udev->context can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }

    ctx = udev->context;
    udev->ready = 0;

    ret = ch375_test_connect(ctx, &conn_status);
    if (ret != CH375_SUCCESS) {
        ERROR("test connect failed, ret=%d", ret);
        goto error;
    }
    if (conn_status == CH375_USB_INT_DISCONNECT) {
        ERROR("udev init failed, device disconnected");
        goto disconnect;
    }

    ret = ch375_get_dev_speed(ctx, &speed);
    if (ret != CH375_SUCCESS) {
        ERROR("get dev speed failed, ret=%d", ret);
        goto disconnect;
    }
    if (speed == CH375_USB_SPEED_LOW) {
        udev->speed = USB_SPEED_LOW;
        INFO("device speed low");
    } else {
        udev->speed = USB_SPEED_FULL;
        INFO("device speed full");
    }

    ret = ch375_set_usb_mode(ctx, CH375_USB_MODE_RESET);
    if (ret != CH375_SUCCESS) {
        ERROR("set usbmode 0x%02X for reset device failed, ret=%d",
            CH375_USB_MODE_RESET, ret);
        goto error;
    }
    // wait 15ms for device reset done, TODO
    HAL_Delay(15);

    ret = ch375_set_usb_mode(ctx, CH375_USB_MODE_SOF);
    if (ret != CH375_SUCCESS) {
        ERROR("set usbmode 0x%02X for automatic sof generation failed, ret=%d",
            CH375_USB_MODE_SOF, ret);
        goto error;
    }

    ret = ch375_host_wait_device_connect(ctx, RESET_WAIT_DEVICE_RECONNECT_TIMEOUT_MS);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("wait device connect failed, reason=%s ret=%d",
            ret == CH375_HST_ERRNO_TIMEOUT ? "timeout": "unkown",
            ret);
        // set usb mode to CH375_USB_MODE_SOF, wait device connect.
        ret = ch375_set_usb_mode(ctx, CH375_USB_MODE_SOF);
        if (ret != CH375_SUCCESS) {
            ERROR("set usb mode=0x%02X failed, ret=%d", CH375_USB_MODE_SOF, ret);
            goto error;
        }
        goto disconnect;
    }

    ret = ch375_set_dev_speed(ctx, speed);
    if (ret != CH375_SUCCESS) {
        ERROR("set device speed(0x%02X) faield, ret=%d", speed, ret);
        goto disconnect;
    }

    HAL_Delay(250); // wait device connection is stable

    udev->connected = 1;
    udev->ready = 1;
    return CH375_HST_ERRNO_SUCCESS;

error:
    udev->connected = 0;
    udev->ready = 0;
    return CH375_HST_ERRNO_ERROR;

disconnect:
    udev->connected = 0;
    udev->ready = 0;
    return CH375_HST_ERRNO_DEV_DISCONNECT;
}

int ch375_host_udev_init(CH375Context *context, USBDevice *udev)
{
    USBDeviceDescriptor dev_desc = {0};
    int ret;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }

    udev->context = context;
    ret = ch375_host_reset_dev(udev);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("reset device failed, ret=%d", ret);
        return ret;
    }
    
    INFO("ready to get device descriptor");

    ret = get_device_descriptor(udev, (uint8_t *)&dev_desc);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("get device descriptor failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    
    udev->context = context;
    udev->ep0_maxpack = dev_desc.bMaxPacketSize0;
    
    udev->vid = ch375_le16_to_cpu(dev_desc.idVendor);
    udev->pid = ch375_le16_to_cpu(dev_desc.idProduct);
    INFO("device pvid = %04X:%04X", udev->vid, udev->pid);
    INFO("ep zero maxpacksize = %d", (int)udev->ep0_maxpack);

    return CH375_HST_ERRNO_SUCCESS;
}

/**
 * @brief 
 * 
 * @param context 
 * @param timeout milliseconds
 * @return int 
 * 
 * TODO :inaccurate timing
 */
int ch375_host_wait_device_connect(CH375Context *context, uint32_t timeout)
{
    int ret;
    uint8_t conn_status;
    uint32_t cnt;
    
    for (cnt = 0; cnt <= timeout; cnt++) {
        ret = ch375_test_connect(context, &conn_status);
        if (ret != CH375_SUCCESS) {
            ERROR("test connect failed, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
        if (conn_status == CH375_USB_INT_DISCONNECT) {
            continue;
        }
        // wait 250ms for the device connection to stabilize
        HAL_Delay(250);
        // conn_status must be CH375_USB_INT_CONNECT or CH375_USB_INT_READY
        return CH375_HST_ERRNO_SUCCESS;
    }

    return CH375_HST_ERRNO_TIMEOUT;
}

int ch375_host_init(CH375Context *context)
{
    int ret;

    INFO("delay 50ms before init");
    HAL_Delay(50); // wait ch375 init done

    ret = ch375_check_exist(context);
    if (ret != CH375_SUCCESS) {
        ERROR("check exist failed %d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    INFO("check ch375 exist done");

    // set usb mode to CH375_USB_MODE_SOF, no sof wait device connect.
    ret = ch375_set_usb_mode(context, CH375_USB_MODE_SOF);
    if (ret != CH375_SUCCESS) {
        ERROR("set usb mode=0x%02X failed, ret=%d", CH375_USB_MODE_SOF, ret);
        return CH375_HST_ERRNO_ERROR;
    }
    INFO("set usb mode to Host, Auto-detection, No SOF");

    return CH375_HST_ERRNO_SUCCESS;
}