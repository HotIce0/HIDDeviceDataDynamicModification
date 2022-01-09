#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "usb.h"
#include "ch375.h"
#include "ch375_usbhost.h"


#define RESET_WAIT_DEVICE_RECONNECT_TIMEOUT_MS 1000

#define TRANSFER_BUFLEN 64

int ch375_host_reset_dev(USBDevice *udev)
{
    int ret;
    uint8_t conn_status;
    uint8_t speed;
    CH375Context *ctx = udev->context;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }

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
        // set usb mode to 0x05, no sof wait device connect.
        ret = ch375_set_usb_mode(ctx, CH375_USB_MODE_NO_SOF);
        if (ret != CH375_SUCCESS) {
            ERROR("set usb mode=0x%02X failed, ret=%d", CH375_USB_MODE_NO_SOF, ret);
            goto error;
        }
        goto disconnect;
    }

    HAL_Delay(250); // wait device connection is stable

    ret = ch375_set_dev_speed(ctx, speed);
    if (ret != CH375_SUCCESS) {
        ERROR("set device speed(0x%02X) faield, ret=%d", speed, ret);
        goto disconnect;
    }

    HAL_Delay(50); // wait device connection is stable

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
    uint8_t buf[TRANSFER_BUFLEN] = {0};
    int ret;
    int actual_len = 0;
    

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
    
    DEBUG("ready to get device descriptor");

    ret = ch375_get_descriptor(context, CH375_USB_DESC_TYPE_DEV_DESC, buf, TRANSFER_BUFLEN, &actual_len);
    if (ret != CH375_SUCCESS) {
        ERROR("get device descriptor failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }

    if (actual_len != DEVICE_DESC_LEN) {
        ERROR("get device descriptor failed, actual_len=%d", actual_len);
        return CH375_HST_ERRNO_ERROR;
    }
    
    udev->context = context;
    udev->ep0_maxpack = buf[7];
    udev->vid = (buf[9] << 8) | buf[8];
    udev->pid = (buf[11] << 8) | buf[10];
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

    // set usb mode to CH375_USB_MODE_NO_SOF, no sof wait device connect.
    ret = ch375_set_usb_mode(context, CH375_USB_MODE_NO_SOF);
    if (ret != CH375_SUCCESS) {
        ERROR("set usb mode=0x%02X failed, ret=%d", CH375_USB_MODE_NO_SOF, ret);
        return CH375_HST_ERRNO_ERROR;
    }
    INFO("set usb mode to Host, Auto-detection, No SOF");

    return CH375_HST_ERRNO_SUCCESS;
}