#ifndef CH375_INTERFACE_H
#define CH375_INTERFACE_H

#include <stdint.h>
typedef struct CH375Context CH375Context;

#define CH375_CMD(_x) ((uint16_t)((_x) | 0x0100))
#define CH375_DATA(_x) ((uint16_t)((_x) | 0x0000))

enum CH375_ERRNO{
    CH375_SUCCESS = 0,
    CH375_ERROR = -1,
    CH375_PARAM_INVALID = -2,
    CH375_WRITE_CMD_FAILD = -3,
    CH375_READ_DATA_FAILD = -4,
    CH375_NO_EXIST = -5,
    CH375_TIMEOUT = -6,
    CH375_NOT_FOUND = -7,    // can't find ch375 board.
};

// Demo code
#if 0
typedef struct CH375Priv {
  UART_HandleTypeDef *huart;
  GPIO_TypeDef *gpio_int;
  uint16_t gpio_pin_int;
} CH375Priv;

static int ch375_func_write_cmd(CH375Context *context, uint8_t cmd)
{
  assert(context);
  CH375Priv *priv = ch375_get_priv(context);
  uint16_t buf = CH375_CMD(cmd);
  uint8_t status;

  status = HAL_UART_Transmit(priv->huart, (uint8_t *)&buf, 1, HAL_MAX_DELAY);
  if (status != HAL_OK) {
      return CH375_ERROR;
  }
  DEBUG("send cmd: origin data=0x%04X", buf);
  return CH375_SUCCESS;
}

static int ch375_func_write_data(CH375Context *context, uint8_t data)
{
  assert(context);
  CH375Priv *priv = ch375_get_priv(context);
  uint16_t buf = CH375_DATA(data);
  uint8_t status;

  status = HAL_UART_Transmit(priv->huart, (uint8_t *)&buf, 1, HAL_MAX_DELAY);
  if (status != HAL_OK) {
      return CH375_ERROR;
  }
  DEBUG("send cmd: origin data=0x%04X", buf);
  return CH375_SUCCESS;
}

static int ch375_func_read_data(CH375Context *context, uint8_t *data)
{
  assert(context);
  CH375Priv *priv = ch375_get_priv(context);
  uint16_t buf = 0;
  uint8_t status;

  status = HAL_UART_Receive(priv->huart, (uint8_t *)&buf, 1, HAL_MAX_DELAY);
  if (status != HAL_OK) {
      ERROR("uart receive failed %d", status);
      return CH375_ERROR;
  }
  DEBUG("receive data: origin data=0x%04X", buf);
  *data = 0xFF & buf;
  return CH375_SUCCESS;
}

// @return 0 not intrrupt, other interrupted
static int ch375_func_query_int(CH375Context *context)
{
  assert(context);
  CH375Priv *priv = ch375_get_priv(context);
  uint8_t val = HAL_GPIO_ReadPin(priv->gpio_int, priv->gpio_pin_int);
  return val == 0 ? 1 : 0;
}
#endif

typedef int (*func_write_cmd)(CH375Context *context, uint8_t cmd);
typedef int (*func_write_data)(CH375Context *context, uint8_t data);
typedef int (*func_read_data)(CH375Context *context, uint8_t *data);
// @return 0 not intrrupt, other interrupted
typedef int (*func_query_int)(CH375Context *context);

void *ch375_get_priv(CH375Context *context);
int ch375_close_context(CH375Context *context);
int ch375_open_context(CH375Context **context, 
    func_write_cmd write_cmd,
    func_write_data write_data,
    func_read_data read_data,
    func_query_int query_int,
    void *priv);

#endif /* CH375_INTERFACE_H */