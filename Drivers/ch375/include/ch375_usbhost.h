#ifndef CH375_USBHOST_H
#define CH375_USBHOST_H

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "ch375_interface.h"
#include "usb.h"


#define USB_SPEED_LOW 0
#define USB_SPEED_FULL 1

#define MAX_ENDPOINT_NUM 3
#define MAX_INTERFACE_NUM 3

enum CH375_HST_ERRNO {
    CH375_HST_ERRNO_SUCCESS = 0,
    CH375_HST_ERRNO_ERROR = -1,
    CH375_HST_ERRNO_PARAM_INVALID = -2,
    CH375_HST_ERRNO_SEND_CMD_FAILD = -3,
    CH375_HST_ERRNO_RECV_DATA_FAILD = -4,
    CH375_HST_ERRNO_NO_EXIST = -5,
    CH375_HST_ERRNO_TIMEOUT = -6,
	CH375_HST_ERRNO_DEV_DISCONNECT = -7,
	CH375_HST_ERRNO_STALL = -8,
};

typedef struct USBEndpoint
{
	uint8_t ep_num;   //端点号
	uint8_t tog;      //同步标志位
	uint8_t attr;     //端点属性
	uint8_t maxpack;  //最大包大小（最大支持64）
	uint8_t interval;
} USBEndpoint;

typedef struct USBInterface
{
	uint8_t interface_num;
	uint8_t interface_class; 
	uint8_t subclass;
	uint8_t protocol;
	uint8_t endpoint_cnt;
	USBEndpoint endpoint[MAX_ENDPOINT_NUM];
} USBInterface;

typedef struct USBDevice
{
	CH375Context *context;
	uint8_t connected;
	uint8_t ready;

	// device
	USBDeviceDescriptor raw_dev_desc;
	uint8_t speed;  // USB_SPEED_LOW
	uint8_t addr;
	uint8_t ep0_maxpack;
	uint16_t vid;
	uint16_t pid;

	// config
	uint8_t *raw_conf_desc;
	uint16_t raw_conf_desc_len;
	uint8_t configuration_value;

	uint8_t interface_cnt;
	USBInterface interface[MAX_INTERFACE_NUM];
} USBDevice;

// transfer api
int ch375_host_control_transfer(USBDevice *udev,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	uint8_t *data, uint16_t wLength, int *actual_length, uint32_t timeout);

int ch375_host_bulk_transfer(USBDevice *udev,
	uint8_t ep, uint8_t *data, int length,
	int *actual_length, uint32_t timeout);

int ch375_host_interrupt_transfer(USBDevice *udev,
	uint8_t ep, uint8_t *data, int length,
	int *actual_length, uint32_t timeout);

// device operation api
int ch375_host_clear_stall(USBDevice *udev, uint8_t ep);
int ch375_host_set_configration(USBDevice *udev, uint8_t iconfigration);
int ch375_host_reset_dev(USBDevice *udev);

void ch375_host_udev_close(USBDevice *udev);
int ch375_host_udev_open(CH375Context *context, USBDevice *udev);
int ch375_host_wait_device_connect(CH375Context *context, uint32_t timeout);

int ch375_host_init(CH375Context *context);

#endif /* CH375_USBHOST_H */