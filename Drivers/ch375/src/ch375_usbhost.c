#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "ch375.h"
#include "ch375_usbhost.h"


#define SETUP_IN(x) ((x) & 0x80)
#define EP_IN(x) ((x) & 0x80)

#define RESET_WAIT_DEVICE_RECONNECT_TIMEOUT_MS 1000
#define TRANSFER_TIMEOUT 5000

#define USB_DEFAULT_ADDRESS 1
#define USB_DEFAULT_EP0_MAX_PACKSIZE 8

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

int ch375_host_control_transfer(USBDevice *udev,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	uint8_t *data, uint16_t wLength, int *actual_length, uint32_t timeout)
{
    CH375Context *ctx;
    uint8_t setup_buf[CONTROL_SETUP_SIZE];
    int residue_len = wLength;
    uint8_t tog = 0;
    int offset = 0;
    uint8_t status;
    int ret;
    
    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (udev->context == NULL) {
        ERROR("param udev->context can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (data == NULL && wLength != 0) {
        ERROR("param data, wLength, is invalied");
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

    ret = ch375_send_token(ctx, 0, tog, USB_PID_SETUP, &status);
    if (ret != CH375_SUCCESS) {
        ERROR("send token(SETUP) failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    if (status != CH375_USB_INT_SUCCESS) {
        ERROR("send token(SETUP) failed, status=0x%02X", status);
        goto status_error;
    }
    tog = tog^1;
    DEBUG("send token(SETUP) done");

    // DATA0
    while (residue_len) {
        uint8_t len = residue_len > udev->ep0_maxpack ? udev->ep0_maxpack: residue_len;
        uint8_t actual_len;
        
        if (SETUP_IN(request_type)) {
            // IN
            ret = ch375_send_token(ctx, 0, tog, USB_PID_IN, &status);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(IN) failed, ret=%d", ret);
                return CH375_HST_ERRNO_ERROR;
            }
            if (status != CH375_USB_INT_SUCCESS) {
                ERROR("send token(IN) failed, status=0x%02X", status);
                goto status_error;
            }
            tog = tog^1;
            
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
            ret = ch375_send_token(ctx, 0, tog, USB_PID_OUT, &status);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(OUT) failed, ret=%d", ret);
                return CH375_HST_ERRNO_ERROR;
            }
            if (status != CH375_USB_INT_SUCCESS) {
                ERROR("send token(OUT) failed, status=0x%02X", status);
                goto status_error;
            }
            tog = tog^1;
            residue_len -= actual_len;
            offset += actual_len;
        }
    }
    // ACK
    tog = 1;
    if (SETUP_IN(request_type)) {
        ret = ch375_write_block_data(ctx, data + offset, 0);
        if (ret != CH375_SUCCESS) {
            ERROR("write control OUT 2 data failed, len=0, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
        ret = ch375_send_token(ctx, 0, tog, USB_PID_OUT, &status);
        if (ret != CH375_SUCCESS) {
            ERROR("send token(OUT 2) failed, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
        if (status != CH375_USB_INT_SUCCESS) {
            ERROR("send token(OUT 2) failed, status=0x%02X", status);
            goto status_error;
        }
    } else {
        ret = ch375_send_token(ctx, 0, tog, USB_PID_IN, &status);
        if (ret != CH375_SUCCESS) {
            ERROR("send token(IN 2) failed, ret=%d", ret);
            return CH375_HST_ERRNO_ERROR;
        }
        if (status != CH375_USB_INT_SUCCESS) {
            ERROR("send token(IN 2) failed, status=0x%02X", status);
            goto status_error;
        }
    }
    *actual_length = offset;
    return CH375_HST_ERRNO_SUCCESS;

status_error:
    if (status == CH375_USB_INT_DISCONNECT) {
        return CH375_HST_ERRNO_DEV_DISCONNECT;
    }
    if (status == CH375_PID2STATUS(CH375_USB_PID_STALL)) {
        return CH375_HST_ERRNO_STALL;
    }
    ERROR("unhandled status=0x%02X", status);
    return CH375_HST_ERRNO_ERROR;
}

static int get_ep(USBDevice *udev, uint8_t ep_addr, USBEndpoint **ep)
{
    assert(udev);
    assert(ep_addr != 0);
    assert(ep);
    int i, j;
    
    for (i = 0; i < udev->interface_cnt; i++) {
        USBInterface *interface = &udev->interface[i];
        for (j = 0; j < interface->endpoint_cnt; j++) {
            if (interface->endpoint[j].ep_num == ep_addr) {
                *ep = &interface->endpoint[j];
                return 0;
            }
        }
    }
    return -1;
}

int ch375_host_bulk_transfer(USBDevice *udev,
	uint8_t ep, uint8_t *data, int length,
	int *actual_length, uint32_t timeout)
{
    CH375Context *ctx = 0;
    USBEndpoint *endpoint = 0;
    int residue_len = length;
    int offset = 0;
    uint8_t status;
    int ret;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (udev->context == NULL) {
        ERROR("param udev->context can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    if (data == NULL && length != 0) {
        ERROR("param data, length, is invalied");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    ctx = udev->context;

    ret = get_ep(udev, ep, &endpoint);
    if (ret < 0) {
        ERROR("param ep(0x%02X) is invalid, find endpoint failed", ep);
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    ret = ch375_set_retry(ctx, CH375_SET_RETRY_TIMES_ZROE);
    if (ret != CH375_SUCCESS) {
        ERROR("set ch375 retry infinity failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }

    while (residue_len > 0) {
        uint8_t len = residue_len > endpoint->maxpack ? endpoint->maxpack: residue_len;
        uint8_t actual_len = 0;
        endpoint->tog = endpoint->tog ^ 1;
        if (EP_IN(ep)) {
            ret = ch375_send_token(ctx, ep, endpoint->tog, USB_PID_IN, &status);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(IN) to ep(0x%02X) faield", ep);
                return CH375_HST_ERRNO_ERROR;
            }
            if (status == CH375_USB_INT_SUCCESS) {
                ret = ch375_read_block_data(ctx, data + offset, len, &actual_len);
                if (ret != CH375_SUCCESS) {
                    ERROR("read IN data(len=%d) failed, ret=%d", len, ret);
                    return CH375_HST_ERRNO_ERROR;
                }
            }
        } else {
            ret = ch375_write_block_data(ctx, data + offset, len);
            if (ret != CH375_SUCCESS) {
                ERROR("write OUT data(len=%d) failed, ret=%d", len, ret);
                return CH375_HST_ERRNO_ERROR;
            }
            ret = ch375_send_token(ctx, ep, endpoint->tog, USB_PID_OUT, &status);
            if (ret != CH375_SUCCESS) {
                ERROR("send token(OUT) to ep(0x%02X) faield", ep);
                return CH375_HST_ERRNO_ERROR;
            }
            if (status == CH375_USB_INT_SUCCESS) {
                ;
            }
        }
        // UPDATE offset, residue...
        if (status == CH375_USB_INT_SUCCESS) {
            endpoint->tog = endpoint->tog ^ 1;
            offset += actual_len;
            residue_len -= actual_len;
            continue;
        }

        if (status == CH375_PID2STATUS(CH375_USB_PID_NAK)){
            if (timeout == 0) {
                ERROR("send token(IN) to ep(0x%02X) timeout, status=0x%02X", ep, status);
                return CH375_HST_ERRNO_TIMEOUT;
            }
            timeout--;
            HAL_Delay(1);
        } else {
            ERROR("send token(IN) to ep(0x%02X) failed, status=0x%02X", ep, status);
            goto status_error;
        }
    }

    *actual_length = offset;
    return CH375_HST_ERRNO_SUCCESS;
status_error:
    if (status == CH375_USB_INT_DISCONNECT) {
        return CH375_HST_ERRNO_DEV_DISCONNECT;
    }
    if (status == CH375_PID2STATUS(CH375_USB_PID_STALL)) {
        return CH375_HST_ERRNO_STALL;
    }
    ERROR("unhandled status=0x%02X", status);
    return CH375_HST_ERRNO_ERROR;
}

int ch375_host_interrupt_transfer(USBDevice *udev,
	uint8_t ep, uint8_t *data, int length,
	int *actual_length, uint32_t timeout)
{
    return ch375_host_bulk_transfer(udev, ep, data, length, actual_length, timeout);
}

static void parser_endpoint_descriptor(USBInterface *interface, USBEndpointDescriptor *desc)
{
    assert(interface);
    assert(desc);

    USBEndpoint *ep = &interface->endpoint[interface->endpoint_cnt];
    ep->ep_num = desc->bEndpointAddress;
    ep->tog = 0;
    ep->attr = desc->bmAttributes;
    ep->maxpack = ch375_le16_to_cpu(desc->wMaxPacketSize);
    ep->interval = desc->bInterval;

    interface->endpoint_cnt++;
}

static void parser_interface_descriptor(USBDevice *udev, USBInterfaceDescriptor *desc)
{
    assert(udev);
    assert(desc);

    USBInterface *interface = &udev->interface[udev->interface_cnt];
    interface->interface_num = desc->bInterfaceNumber;
    interface->interface_class = desc->bInterfaceClass;
    interface->subclass = desc->bInterfaceSubClass;
    interface->protocol = desc->bInterfaceProtocol;

    udev->interface_cnt++;
}

static int parser_config_descriptor(USBDevice *udev)
{
    USBDescriptor *desc = (USBDescriptor *)udev->raw_conf_desc;
    void *raw_conf_desc_end = (uint8_t *)udev->raw_conf_desc + udev->raw_conf_desc_len;
    
    while ((void *)desc < raw_conf_desc_end) {
        // first run to skip configration descriptor.
        desc = (USBDescriptor *)((uint8_t *)desc + desc->bLength);

        switch (desc->bDesriptorType) {
            case USB_DT_INTERFACE: {
                (void)parser_interface_descriptor(udev, (USBInterfaceDescriptor *)desc);
                break;
            }
            case USB_DT_ENDPOINT: {
                uint8_t last_interface_idx;
                if (udev->interface_cnt == 0) {
                    ERROR("invalid config descriptor, the endpoint desc is not behind the interface desc");
                    return CH375_HST_ERRNO_ERROR;
                }
                last_interface_idx = udev->interface_cnt - 1;
                (void)parser_endpoint_descriptor(&udev->interface[last_interface_idx], (USBEndpointDescriptor *)desc);
                break;
            }
            default: {
                break;
            }
        }
    }
    return CH375_HST_ERRNO_SUCCESS;
}

static int get_config_descriptor(USBDevice *udev, uint8_t *buf, uint16_t len)
{
    assert(udev);
    assert(buf);
    
    int actual_len = 0;
    int ret;

    assert(len >= sizeof(USBConfigDescriptor));

    ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_IN | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE,
        USB_REQUEST_GET_DESCRIPTOR,
        USB_DT_CONFIG << 8, 0,
        buf, len, &actual_len, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("control transfer failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    if (actual_len < len) {
        ERROR("no enough data");
        return CH375_HST_ERRNO_ERROR;
    }
    return CH375_HST_ERRNO_SUCCESS;
}

static int ch375_set_dev_address(USBDevice *udev, uint8_t addr)
{
    assert(udev);
    assert(addr != 0);

    int ret;
    // send setup of set_address to device
    ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_OUT | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE,
        USB_REQUEST_SET_ADDRESS,
        addr, 0, NULL, 0, NULL, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("control transfer failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }

    // tell CH375 the address of device
    ret = ch375_set_usb_addr(udev->context, addr);
    if (ret != CH375_SUCCESS) {
        ERROR("set ch375 usb addr(0x%02X) failed, ret=%d", addr, ret);
        return CH375_HST_ERRNO_ERROR;
    }

    return CH375_HST_ERRNO_SUCCESS;
}

static int get_device_descriptor(USBDevice *udev, uint8_t *buf)
{
    assert(buf);
    int actual_len = 0;
    int ret;
    USBDeviceDescriptor *dev_desc = (USBDeviceDescriptor *)buf;
    
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
    if (dev_desc->bDescriptorType != USB_DT_DEVICE) {
        ERROR("receive invalid deivce descriptor, desc_type=0x%02X, expect=0x%02X",
            dev_desc->bDescriptorType, USB_DT_DEVICE);
        return CH375_HST_ERRNO_ERROR;
    }
    return CH375_HST_ERRNO_SUCCESS;
}

int ch375_host_clear_stall(USBDevice *udev, uint8_t ep)
{
    USBEndpoint *endpoint = NULL;
    int ret;
    
    if (ep != 0) {
        ret = get_ep(udev, ep, &endpoint);
        if (ret < 0) {
            ERROR("param ep=0x%02X is invalied", ep);
            return CH375_HST_ERRNO_PARAM_INVALID;
        }
    }

    ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_IN | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_ENDPOINT,
        USB_REQUEST_CLEAR_FEATURE,
        0, ep, NULL, 0, NULL, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("send clear feature request failed, ret=%d", ret);
        return CH375_HST_ERRNO_ERROR;
    }
    if (endpoint) {
        endpoint->tog = 0;
    }

    return CH375_HST_ERRNO_SUCCESS;
}

int ch375_host_set_configration(USBDevice *udev, uint8_t iconfigration)
{
    int ret = ch375_host_control_transfer(udev,
        USB_ENDPOINT_OUT | USB_REQUEST_TYPE_STANDARD | USB_RECIPIENT_DEVICE,
        USB_REQUEST_SET_CONFIGURATION,
        iconfigration, 0, NULL, 0, NULL, TRANSFER_TIMEOUT);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("send set_configration(iconfigration=%d) request failed, ret=%d",
            iconfigration, ret);
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

void ch375_host_udev_close(USBDevice *udev)
{
    //TODO
    return;
}

int ch375_host_udev_open(CH375Context *context, USBDevice *udev)
{
    USBConfigDescriptor short_conf_desc = {0};
    uint16_t conf_total_len = 0;
    int ret;
    int i;
    uint8_t ep_cnt = 0;

    if (udev == NULL) {
        ERROR("param udev can't be NULL");
        return CH375_HST_ERRNO_PARAM_INVALID;
    }
    
    memset(udev, 0, sizeof(USBDevice));
    udev->context = context;
    udev->ep0_maxpack = USB_DEFAULT_EP0_MAX_PACKSIZE;

    ret = ch375_host_reset_dev(udev);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("reset device failed, ret=%d", ret);
        return ret;
    }

    INFO("ready to get device descriptor");
    ret = get_device_descriptor(udev, (uint8_t *)&udev->raw_dev_desc);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("get device descriptor failed, ret=%d", ret);
        goto failed;
    }

    udev->ep0_maxpack = udev->raw_dev_desc.bMaxPacketSize0;
    udev->vid = ch375_le16_to_cpu(udev->raw_dev_desc.idVendor);
    udev->pid = ch375_le16_to_cpu(udev->raw_dev_desc.idProduct);
    
    INFO("device pvid = %04X:%04X", udev->vid, udev->pid);
    INFO("ep 0 max packsize = %d", (int)udev->ep0_maxpack);

    INFO("ready to set address");
    // set device address
    ret = ch375_set_dev_address(udev, USB_DEFAULT_ADDRESS);
    if (ret != CH375_HST_ERRNO_SUCCESS)  {
        ERROR("set device address failed, ret=%d", ret);
        goto failed;
    }
    
    INFO("ready to get config descriptor");
    // get short config descriptor for wTotalLength
    ret = get_config_descriptor(udev, (uint8_t *)&short_conf_desc, sizeof(short_conf_desc));
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("get short device descriptor failed, ret=%d", ret);
        goto failed;
    }
    
    conf_total_len = ch375_le16_to_cpu(short_conf_desc.wTotalLength);
    udev->configuration_value = short_conf_desc.bConfigurationValue;
    udev->raw_conf_desc_len = conf_total_len;
    INFO("config wTotalLength=%d", conf_total_len);
    
    // get full config descriptor
    udev->raw_conf_desc = (uint8_t *)malloc(conf_total_len);
    if (udev->raw_conf_desc == NULL) {
        ERROR("alloc config descriptor buffer failed");
        goto failed;
    }
    memset(udev->raw_conf_desc, 0, conf_total_len);

    ret = get_config_descriptor(udev, udev->raw_conf_desc, conf_total_len);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("get full config descriptor failed, ret=%d", ret);
        goto failed;
    }
    // analyse config descriptor
    ret = parser_config_descriptor(udev);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("parser config descritptor failed, ret=%d", ret);
        goto failed;
    }

    for (i = 0; i < udev->interface_cnt; i++) {
        ep_cnt += udev->interface[i].endpoint_cnt;
    }
    INFO("deivce have %d interface, %d endpoint", udev->interface_cnt, ep_cnt);
    
    ret = ch375_host_set_configration(udev, udev->configuration_value);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
        ERROR("set configration(%d) failed, ret=%d", udev->configuration_value, ret);
        goto failed;
    }
    INFO("set configration %d success", udev->configuration_value);
    udev->connected = 1;

    return CH375_HST_ERRNO_SUCCESS;
failed:
    if (udev->raw_conf_desc != NULL) {
        free(udev->raw_conf_desc);
    }
    memset(udev, 0, sizeof(USBDevice));
    return CH375_HST_ERRNO_ERROR;
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