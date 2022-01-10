#ifndef CH375_H
#define CH375_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#include "ch375_interface.h"

/*
模式代码为 00H 时切换到未启用的 USB 设备方式（上电或复位后的默认方式）；
模式代码为 01H 时切换到已启用的 USB 设备方式，外部固件模式；
模式代码为 02H 时切换到已启用的 USB 设备方式，内置固件模式；
模式代码为 04H 时切换到未启用的 USB 主机方式；
模式代码为 05H 时切换到已启用的 USB 主机方式，不产生 SOF 包；
模式代码为 06H 时切换到已启用的 USB 主机方式，自动产生 SOF 包；
模式代码为 07H 时切换到已启用的 USB 主机方式，复位 USB 总线；

建议在没有 USB 设备时使用模式 5，插入 USB 设备后先进入模式 7 再换到模式 6。
通常情况下，设置 USB 工作模式在 20uS 时间之内完成，完成后输出操作状态
 */
#define CH375_USB_MODE_NO_DETECT 0x04
#define CH375_USB_MODE_NO_SOF    0x05
#define CH375_USB_MODE_SOF       0x06
#define CH375_USB_MODE_RESET     0x07

/**
 * @brief host mode, int status code
 */
#define	CH375_USB_INT_SUCCESS		0x14			/* USB事务或者传输操作成功 */
#define	CH375_USB_INT_CONNECT		0x15			/* 检测到USB设备连接事件 */
#define	CH375_USB_INT_DISCONNECT	0x16			/* 检测到USB设备断开事件 */
#define	CH375_USB_INT_BUF_OVER	0x17			/* USB控制传输的数据太多, 缓冲区溢出 */
#define	CH375_USB_INT_USB_READY	0x18			/* USB设备已经被初始化（已分配USB地址） */
#define	CH375_USB_INT_DISK_READ	0x1D			/* USB存储器读数据块, 请求数据读出 */
#define	CH375_USB_INT_DISK_WRITE	0x1E			/* USB存储器写数据块, 请求数据写入 */
#define	CH375_USB_INT_DISK_ERR	0x1F			/* USB存储器操作失败 */

/**
 * @brief 
 */
#define CH375_USB_DESC_TYPE_DEV_DESC    0x01
#define CH375_USB_DESC_TYPE_CONFIG_DESC 0x02

#define CH375_USB_SPEED_LOW  1
#define CH375_USB_SPEED_FULL 0

#define CH375_SET_RETRY_TIMES_ZROE      0x00
#define CH375_SET_RETRY_TIMES_2MS       0x01
#define CH375_SET_RETRY_TIMES_INFINITY  0x02


static inline uint16_t ch375_cpu_to_le16(const uint16_t x)
{
	union {
		uint8_t  b8[2];
		uint16_t b16;
	} _tmp;
	_tmp.b8[1] = (uint8_t) (x >> 8);
	_tmp.b8[0] = (uint8_t) (x & 0xff);
	return _tmp.b16;
}

#define ch375_le16_to_cpu ch375_cpu_to_le16

// Any Mode
int ch375_query_int(CH375Context *context);
int ch375_wait_int(CH375Context *context, uint32_t timeout);

int ch375_write_cmd(CH375Context *context, uint8_t cmd);
int ch375_read_data(CH375Context *context, uint8_t *data);
int ch375_write_block_data(CH375Context *context, uint8_t *buf, uint8_t len);
int ch375_read_block_data(CH375Context *context, uint8_t *buf, uint8_t len, uint8_t *actual_len);


int ch375_check_exist(CH375Context *context);
int ch375_get_version(CH375Context *context, uint8_t *version);
int ch375_set_usb_mode(CH375Context *context, uint8_t mode);
int ch375_get_status(CH375Context *context, uint8_t *status);
int ch375_abort_nak(CH375Context *context);

// Host Mode
int ch375_test_connect(CH375Context *context, uint8_t *connect_status);
int ch375_get_dev_speed(CH375Context *context, uint8_t *speed);
int ch375_set_dev_speed(CH375Context *context, uint8_t speed);

/**
 * @brief 
 * @param context 
 * @param times CH375_SET_RETRY_TIMES_ZROE, CH375_SET_RETRY_TIMES_2MS, CH375_SET_RETRY_TIMES_INFINITY
 */
int ch375_set_retry(CH375Context *context, uint8_t times);
int ch375_send_token(CH375Context *context, uint8_t ep, uint8_t tog, uint8_t pid);


void *ch375_get_priv(CH375Context *context);

int ch375_close_context(CH375Context *context);
int ch375_open_context(CH375Context **context, 
    func_write_cmd write_cmd,
    func_write_data write_data,
    func_read_data read_data,
    func_query_int query_int,
    void *priv);

#endif /* CH375_H */