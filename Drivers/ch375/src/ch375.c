#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"
#include "ch375.h"

#define WAIT_INT_TIMEOUT_MS (10 * 1000)

/**
 * @brief commands
 */
#define CH375_CMD_GET_IC_VER 0x01
#define CH375_CMD_SET_USB_SPEED 0x04
#define CH375_CMD_CHECK_EXIST 0x06
#define	CH375_CMD_GET_DEV_RATE	0x0A
#define	CH375_CMD_SET_RETRY 0x0B			/* 主机方式: 设置USB事务操作的重试次数 */
#define CH375_CMD_SET_USB_MODE 0x15
#define CH375_CMD_TEST_CONNECT 0x16
#define CH375_CMD_ABORT_NAK 0x17
#define CH375_CMD_GET_STATUS 0x22
#define	CH375_CMD_UNLOCK_USB 0x23		/* 设备方式: 释放当前USB缓冲区 */
#define	CH375_CMD_RD_USB_DATA0 0x27			/* 从当前USB中断的端点缓冲区读取数据块 */
#define	CH375_CMD_RD_USB_DATA 0x28		/* 从当前USB中断的端点缓冲区读取数据块, 并释放缓冲区, 相当于 CMD_RD_USB_DATA0 + CMD_UNLOCK_USB */
#define	CH375_CMD_WR_USB_DATA3 0x29			/* 设备方式: 向USB端点0的发送缓冲区写入数据块 */
#define	CH375_CMD_WR_USB_DATA5 0x2A			/* 设备方式: 向USB端点1的发送缓冲区写入数据块 */
#define	CH375_CMD_WR_USB_DATA7 0x2B			/* 向USB端点2或者主机端点的发送缓冲区写入数据块 */
#define CH375_CMD_GET_DESC  0x46
#define	CH375_CMD_ISSUE_TKN_X 0x4E			/* 主机方式: 发出同步令牌, 执行事务, 该命令可代替 CMD_SET_ENDP6/CMD_SET_ENDP7 + CMD_ISSUE_TOKEN */


/**
 * @brief command result
 */
#define CH375_CMD_RET_SUCCESS 0x51
#define	CH375_CMD_RET_FAILED 0x5F

/**
 * @brief send data1 to ch375, should recevie data2
 */
#define CH375_CHECK_EXIST_DATA1 0x65
#define CH375_CHECK_EIXST_DATA2 ((uint8_t)~CH375_CHECK_EXIST_DATA1)

/**
 * @brief 
 * CH375_CMD_GET_DEV_RATE, data
 */
#define CH375_GET_DEV_RATE_DATA 0x07

/**
 * @brief 
 * CH375_CMD_SET_USB_SPEED, data
 */
#define CH375_SET_USB_SPEED_LOW 0x02
#define CH375_SET_USB_SPEED_FULL 0x00

/**
 * @brief 
 * CH375_CMD_SET_RETRY
 */
#define CH375_SET_RETRY_DATA 0x25


typedef struct CH375Context CH375Context;

struct CH375Context {
    void *priv;
    func_write_cmd write_cmd;
    func_write_data write_data;
    func_read_data read_data;
    func_query_int query_int;
};


int ch375_write_cmd(CH375Context *context, uint8_t cmd)
{
    if (context == NULL) {
        ERROR("param context can't be NULL\n");
        return CH375_PARAM_INVALID;
    }

    return context->write_cmd(context, cmd);
}

int ch375_write_data(CH375Context *context, uint8_t data)
{
    if (context == NULL) {
        ERROR("param context can't be NULL\n");
        return CH375_PARAM_INVALID;
    }

    return context->write_data(context, data);
}

int ch375_read_data(CH375Context *context, uint8_t *data)
{
    if (data == NULL) {
        ERROR("param data can't be NULL");
        return CH375_PARAM_INVALID;
    }
    return context->read_data(context, data);
}

/**
 * @brief 
 * 
 * @param context 
 * @param buf 
 * @param len max value is 64
 * @return int 
 */
int ch375_write_block_data(CH375Context *context, uint8_t *buf, int len)
{
    int ret;
    int offset;

    if (len > 64) {
        ERROR("write block data len is max than 64");
        return CH375_PARAM_INVALID;
    }

    ret = ch375_write_cmd(context, CH375_CMD_WR_USB_DATA7);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    offset = 0;
    while (len > 0) {
        ret = ch375_write_data(context, *(buf + offset));
        if (ret != CH375_SUCCESS) {
            return CH375_WRITE_CMD_FAILD;
        }
        offset++;
        len--;
    }
    return CH375_SUCCESS;
}

/**
 * @brief 
 * 
 * @param context 
 * @param buf 
 * @param len must be greater than 64, avoid overflow
 * @return int (actual type is uint8_t)
 */
int ch375_read_block_data(CH375Context *context, uint8_t *buf, int len, int *actual_len)
{
    int ret;
    uint8_t data_len;
    uint8_t residue_len;
    uint8_t offset;

    if (buf == NULL) {
        ERROR("param buf can't be NULL");
        return CH375_PARAM_INVALID;
    }
    // TODO len check, and read overflowed data
    if (actual_len == NULL) {
        ERROR("param actual_len can't be NULL");
        return CH375_PARAM_INVALID;
    }

    ret = ch375_write_cmd(context, CH375_CMD_RD_USB_DATA);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    ret = ch375_read_data(context, &data_len);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }
    residue_len = data_len;

    offset = 0;
    while (residue_len > 0) {
        ret = ch375_read_data(context, buf + offset);
        if (ret != CH375_SUCCESS) {
            return CH375_READ_DATA_FAILD;
        }
        offset++;
        residue_len--;
    }

    *actual_len = data_len;
    return CH375_SUCCESS;
}

int ch375_get_version(CH375Context *context, uint8_t *version)
{
    uint8_t buf = 0;
    int ret = -1;

    if (version == NULL) {
        ERROR("param version can't be NULL");
        return CH375_PARAM_INVALID;
    }

    ret = ch375_write_cmd(context, CH375_CMD_GET_IC_VER);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_read_data(context, &buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }
    // lower 6 bits is vesion, ref CH375DS1.PDF
    *version = 0x3F & buf;
    return CH375_SUCCESS;
}

int ch375_query_int(GPIO_TypeDef *gpio, uint16_t gpio_pin)
{
    uint8_t val = HAL_GPIO_ReadPin(gpio, gpio_pin);
    return val == 0 ? 1 : 0;
}

int ch375_wait_int(GPIO_TypeDef *gpio, uint16_t gpio_pin, uint32_t timeout)
{
    int cnt;

    for (cnt = 0; cnt < timeout; cnt++) {
        if (ch375_query_int(gpio, gpio_pin)) {
            return CH375_SUCCESS;
        }
        HAL_Delay(1);
    }
    return CH375_TIMEOUT;
}

/**
 * @brief check the ch375 is working fine
 * 
 * @param context 
 * @return int  CH375_SUCCESS
 *              CH375_SEND_CMD_FAILD
 *              CH375_RECV_DATA_FAILD
 *              CH375_NO_EXIST
 */
int ch375_check_exist(CH375Context *context)
{
    uint8_t recv_buf = 0;
    int ret = ch375_write_cmd(context, CH375_CMD_CHECK_EXIST);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_write_data(context, CH375_CHECK_EXIST_DATA1);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_read_data(context, &recv_buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }
    if (recv_buf != CH375_CHECK_EIXST_DATA2) {
        ERROR("receive error check exist data = 0x%02X, should be 0x%02X",
            recv_buf, CH375_CHECK_EIXST_DATA2);
        return CH375_NO_EXIST;
    }
    return CH375_SUCCESS;
}

int ch375_set_usb_mode(CH375Context *context, uint8_t mode)
{
    uint8_t buf = 0;
    int ret;

    ret = ch375_write_cmd(context, CH375_CMD_SET_USB_MODE);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_write_data(context, mode);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    HAL_Delay(1); // 20us to set success, 1ms is enough

    ret = ch375_read_data(context, &buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }

    if (buf != CH375_CMD_RET_SUCCESS) {
        ERROR("set mode failed, ret code=0x%02X", buf);
        return CH375_ERROR;
    }
    return CH375_SUCCESS;
}

int ch375_get_dev_speed(CH375Context *context, uint8_t *speed)
{
    uint8_t buf = 0;
    int ret;

    ret = ch375_write_cmd(context, CH375_CMD_GET_DEV_RATE);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_write_data(context, CH375_GET_DEV_RATE_DATA);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    ret = ch375_read_data(context, &buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }
    if (buf & 0x10) {
        *speed = CH375_USB_SPEED_LOW;
    } else {
        *speed = CH375_USB_SPEED_FULL;
    }
    return CH375_SUCCESS;
}

int ch375_set_dev_speed(CH375Context *context, uint8_t speed)
{
    int ret;
    uint8_t val;

    if (speed != CH375_USB_SPEED_LOW && speed != CH375_USB_SPEED_FULL) {
        ERROR("param speed(0x%02X) invalid", speed);
        return CH375_PARAM_INVALID;
    }

    if (speed == CH375_USB_SPEED_LOW) {
        val = CH375_SET_USB_SPEED_LOW;
    } else {
        val = CH375_SET_USB_SPEED_FULL;
    }

    ret = ch375_write_cmd(context, CH375_CMD_SET_USB_SPEED);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_write_data(context, val);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    return CH375_SUCCESS;
}

#define usb1_int_Pin GPIO_PIN_15
#define usb1_int_GPIO_Port GPIOE

/**
 * @brief 
 * 
 * @param context 
 * @param desc_type CH375_USB_DESC_TYPE_DEV_DESC, CH375_USB_DESC_TYPE_CONFIG_DESC
 * @param buf 
 * @param len 
 * @return int 
 */
int ch375_get_descriptor(CH375Context *context, uint8_t desc_type, uint8_t *buf, int len, int *actual_len)
{
    int ret;
    uint8_t status;

    if (desc_type != CH375_USB_DESC_TYPE_DEV_DESC &&
        desc_type != CH375_USB_DESC_TYPE_CONFIG_DESC) {
        ERROR("param desc_type invalid");
        return CH375_PARAM_INVALID;
    }

    if (buf == NULL) {
        ERROR("param buf can't be NULL");
        return CH375_PARAM_INVALID;
    }

    if (len < 64) {
        ERROR("read buffer must be greater than 64, avoid overflow");
        return CH375_PARAM_INVALID;
    }

    if (actual_len == NULL) {
        ERROR("param actual_len can't be NULL");
        return CH375_PARAM_INVALID;
    }

    ret = ch375_write_cmd(context, CH375_CMD_GET_DESC);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    ret = ch375_write_data(context, desc_type);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    ret = ch375_wait_int(usb1_int_GPIO_Port, usb1_int_Pin, WAIT_INT_TIMEOUT_MS);
    if (ret == CH375_TIMEOUT) {
        ERROR("get descriptor wait int timeout");
        return CH375_ERROR;
    }
    ret = ch375_get_status(context, &status);
    if (ret != CH375_SUCCESS) {
        ERROR("get statsus failed, ret=%d", ret);
        return CH375_ERROR;
    }
    DEBUG("cmd get desc, status=0x%02X", status);

    ret = ch375_read_block_data(context, buf, len, actual_len);
    if (ret != CH375_SUCCESS) {
        ERROR("read descriptor failed");
        return CH375_READ_DATA_FAILD;
    }

    return CH375_SUCCESS;
}

/**
 * @brief 
 * 
 * @param context 
 * @param connect_status CH375_USB_INT_CONNECT, CH375_USB_INT_DISCONNECT, CH375_USB_INT_USB_READY
 * @return 
 */
int ch375_test_connect(CH375Context *context, uint8_t *connect_status)
{
    int ret;
    uint8_t buf;
    uint8_t status;

    if (connect_status == NULL) {
        ERROR("param connect_status can't be NULL");
        return CH375_PARAM_INVALID;
    }
    
    ret = ch375_write_cmd(context, CH375_CMD_TEST_CONNECT);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    HAL_Delay(1);

    ret = ch375_read_data(context, &buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }
    DEBUG("test connect status=0x%02X", buf);
    // buf = CH375_USB_INT_CONNECT, CH375_H_INT_DISCONNECT, CH375_H_INT_USB_READY
    assert(buf == CH375_USB_INT_DISCONNECT ||
        buf == CH375_USB_INT_CONNECT || 
        buf == CH375_USB_INT_USB_READY);

    if (buf == CH375_USB_INT_DISCONNECT) {
        (void)ch375_get_status(context, &status);
    }

    *connect_status = buf;
    return CH375_SUCCESS;
}

int ch375_get_status(CH375Context *context, uint8_t *status)
{
    uint8_t buf = 0;
    int ret = -1;

    if (status == NULL) {
        ERROR("param status can't be NULL");
        return CH375_PARAM_INVALID;
    }

    ret = ch375_write_cmd(context, CH375_CMD_GET_STATUS);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    ret = ch375_read_data(context, &buf);
    if (ret != CH375_SUCCESS) {
        return CH375_READ_DATA_FAILD;
    }

    *status = buf;
    return CH375_SUCCESS;
}

int ch375_abort_nak(CH375Context *context)
{
    int ret;
    ret = ch375_write_cmd(context, CH375_CMD_ABORT_NAK);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    return CH375_SUCCESS;
}

/*bit7 bit6:NAK重试
   1     0  对NAK无限重试
   1     1  对NAK重试200ms-2s
   0     x  直接返回NAK
bit5-bit0 (0-63) :响应超时重试次数（针对所有传输，超时5次）
*/
/* times的值： 0：NAK不重试  1：重试200-2s  >2:无限次重试NAK */
int ch375_set_retry(CH375Context *context, uint8_t times)
{
    int ret;
    uint8_t param;
    ret = ch375_write_cmd(context, CH375_CMD_SET_RETRY);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    ret = ch375_write_data(context, CH375_SET_RETRY_DATA);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    if (times == 0) {
        param = 0x05;
    } else if (times == 1) {
        param = 0xC0;
    } else {
        param = 0x85;
    }

    ret = ch375_write_data(context, param);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }

    return CH375_SUCCESS;
}

int ch375_send_token(CH375Context *context, uint8_t pid, uint8_t ep, uint8_t tog)
{
    int ret;
    uint8_t tog_val;
    uint8_t ep_pid;
    uint8_t status;

    ret = ch375_write_cmd(context, CH375_CMD_ISSUE_TKN_X);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    // 7bits: in ep tog, 6bits: out ep tog, 5~0bits: must be zero
    tog_val = tog ? 0xC0: 0x00;
    ret = ch375_write_data(context, tog_val);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    // 7~4bits: ep, 3~0bits: pid
    ep_pid = ((0xF & ep) << 4) | (0xF & pid);
    ret = ch375_write_data(context, ep_pid);
    if (ret != CH375_SUCCESS) {
        return CH375_WRITE_CMD_FAILD;
    }
    // wait intrrupt, get result of token send.
    ret = ch375_wait_int(usb1_int_GPIO_Port, usb1_int_Pin, WAIT_INT_TIMEOUT_MS);
    if (ret != CH375_SUCCESS) {
        return CH375_TIMEOUT;
    }
    ret = ch375_get_status(context, &status);
    if (ret != CH375_SUCCESS) {
        return CH375_ERROR;
    }
    if (status != CH375_USB_INT_SUCCESS) {
        ERROR("send token(tog=0x%02X, ep_pid=0x%02X) excute failed, status=0x%02X", tog_val, ep_pid, status);
        return CH375_ERROR;
    }

    return CH375_SUCCESS;
}

static int check_context_invalid(CH375Context *context)
{
    assert(context);
    
    if (context->query_int == NULL) {
        ERROR("context->query_int is NULL");
        goto failed;
    }
    if (context->write_cmd == NULL) {
        ERROR("context->write_cmd is NULL");
        goto failed;
    }
    if (context->read_data == NULL) {
        ERROR("context->read_data is NULL");
        goto failed;
    }
    if (context->write_data == NULL) {
        ERROR("context->write_data is NULL");
        goto failed;
    }

    return CH375_SUCCESS;
failed:
    return CH375_ERROR;
}

void *ch375_get_priv(CH375Context *context)
{
    if (context == NULL) {
        return NULL;
    }
    return context->priv;
}

int ch375_close_context(CH375Context *context)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return CH375_PARAM_INVALID;
    }
    memset(context, 0, sizeof(CH375Context));
    free(context);
    return CH375_SUCCESS;
}

int ch375_open_context(CH375Context **context, 
    func_write_cmd write_cmd,
    func_write_data write_data,
    func_read_data read_data,
    func_query_int query_int,
    void *priv)
{
    CH375Context *ctx;
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return CH375_PARAM_INVALID;
    }

    ctx = (CH375Context *)malloc(sizeof(CH375Context));
    memset(ctx, 0, sizeof(CH375Context));

    ctx->priv = priv;
    ctx->write_cmd = write_cmd;
    ctx->write_data = write_data;
    ctx->read_data = read_data;
    ctx->query_int = query_int;

    if (check_context_invalid(ctx) != CH375_SUCCESS) {
        ERROR("check context failed");
        return CH375_PARAM_INVALID;
    }

    *context = ctx;
    return CH375_SUCCESS;
}