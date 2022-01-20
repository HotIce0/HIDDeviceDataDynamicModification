#include <assert.h>

#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "ch375_usbhost.h"
#include "hid/usbhid.h"
#include "hid/hid_mouse.h"
#include "hid/hid_keyboard.h"

#include "usb_device.h"
#include "usbd_customhid.h"
#include "composite_hid.h"

#include "main.h"


typedef struct CH375Priv {
  UART_HandleTypeDef *huart;
  GPIO_TypeDef *gpio_port_int;
  uint16_t gpio_pin_int;
} CH375Priv;

static UART_HandleTypeDef s_huart2; // ch375a
static UART_HandleTypeDef s_huart3; // ch375b
static UART_HandleTypeDef s_huart4; // log

// CH375 CONFIG
#define CH375_WORK_BAUDRATE 115200
#define CH375_DEFAULT_BAUDRATE 9600
#define CH375_MODULE_NUM 2
GPIO_TypeDef *ch375_int_port[CH375_MODULE_NUM] = {ch375a_int_GPIO_Port, ch375b_int_GPIO_Port};
uint16_t ch375_int_pin[CH375_MODULE_NUM] = {ch375a_int_Pin, ch375b_int_Pin};

UART_HandleTypeDef *ch375_huart[CH375_MODULE_NUM] = {0};
CH375Priv ch375_priv[CH375_MODULE_NUM] = {0};
CH375Context *ch375_ctx[CH375_MODULE_NUM] = {0};


static int ch375_func_write_cmd(CH375Context *context, uint8_t cmd)
{
  assert(context);
  CH375Priv *priv = ch375_get_priv(context);
  uint16_t buf = CH375_CMD(cmd);
  uint8_t status;
  
  status = HAL_UART_Transmit(priv->huart, (uint8_t *)&buf, 1, 500);
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

  status = HAL_UART_Transmit(priv->huart, (uint8_t *)&buf, 1, 500);
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

  status = HAL_UART_Receive(priv->huart, (uint8_t *)&buf, 1, 500);//HAL_MAX_DELAY
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
  CH375Priv *priv = (CH375Priv *)ch375_get_priv(context);
  uint8_t val = HAL_GPIO_ReadPin(priv->gpio_port_int, priv->gpio_pin_int);
  return val == 0 ? 1 : 0;
}

static void loop_handle_keyboard(HIDKeyboard *dev)
{
  USBHIDDevice *hiddev = dev->hid_dev;
  uint8_t *report_buf = NULL;
  int ret;

  while (1) {
    ret = hid_keyboard_fetch_report(dev);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("fetch report failed, ret=%d", ret);
      if (ret == USBHID_ERRNO_NO_DEV) {
        return;
      }
    }
    (void)usbhid_get_report_buffer(hiddev, &report_buf, NULL, 0);
    
    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report_buf, hiddev->report_length);
  }
}

static void loop_handle_mosue(HIDMouse *dev)
{
  USBHIDDevice *hiddev = dev->hid_dev;
  uint8_t *report_buf = NULL;
  int ret;

  while (1) {
    ret = hid_mouse_fetch_report(dev);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("fetch report failed, ret=%d", ret);
      if (ret == USBHID_ERRNO_NO_DEV) {
        return;
      }
    }
    (void)usbhid_get_report_buffer(hiddev, &report_buf, NULL, 0);
    
    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report_buf, hiddev->report_length);
  }
}

/**
 * @brief 
 * 
 * @param huart 
 * @param name 
 * @param Instance 
 * @param baudrate 
 * @param eight_bits 1 eight bits, 0 nine bits 
 * @param rx boolean
 * @param tx boolean
 */
static void uart_init(UART_HandleTypeDef *huart, const char *name,
  USART_TypeDef *Instance, uint32_t baudrate, uint8_t eight_bits, uint8_t rx, uint8_t tx)
{
  uint32_t mode = {0};

  if (rx) {
    mode |= UART_MODE_RX;
  }
  if (tx) {
    mode |= UART_MODE_TX;
  }

  huart->Instance = Instance;
  huart->Init.BaudRate = baudrate;
  huart->Init.WordLength = eight_bits ? UART_WORDLENGTH_8B: UART_WORDLENGTH_9B;
  huart->Init.StopBits = UART_STOPBITS_1;
  huart->Init.Parity = UART_PARITY_NONE;
  huart->Init.Mode = mode;
  huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart->Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(huart) != HAL_OK)
  {
    ERROR("USART(%s) Init failed", name);
    Error_Handler();
  }
}

static void uart_deinit(UART_HandleTypeDef *huart, const char *name)
{
  if (HAL_UART_DeInit(huart) != HAL_OK) {
    ERROR("USART(%s) deinit failed", name);
    Error_Handler();
  }
}

void ch375_init(void)
{
  int i;
  int ret;

  ch375_huart[0] = &s_huart2;
  ch375_huart[1] = &s_huart3;
  // UART Init
  uart_init(&s_huart2, "usart2", USART2, CH375_DEFAULT_BAUDRATE, 0, 1, 1);
  uart_init(&s_huart3, "usart3", USART3, CH375_DEFAULT_BAUDRATE, 0, 1, 1);

  // Fill ch375_priv
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ch375_priv[i].huart = ch375_huart[i];
    ch375_priv[i].gpio_port_int = ch375_int_port[i];
    ch375_priv[i].gpio_pin_int = ch375_int_pin[i];
  }
  // Init Context
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ret = ch375_open_context(&ch375_ctx[i],
            ch375_func_write_cmd,
            ch375_func_write_data,
            ch375_func_read_data,
            ch375_func_query_int,
            &ch375_priv[i]);
    if (ret != CH375_SUCCESS) {
      ERROR("open ch375(%d) context failed, ret=%d", i, ret);
      Error_Handler();
    }
  }
  // ch375 USB Host Init
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ret = ch375_host_init(ch375_ctx[i], CH375_WORK_BAUDRATE);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
      ERROR("ch375(%d) init host failed, %d", i, ret);
      Error_Handler();
    }
  }
  // reinit uart to work baudrate
  uart_deinit(&s_huart2, "usart2");
  uart_deinit(&s_huart3, "usart3");
  uart_init(&s_huart2, "usart2", USART2, CH375_WORK_BAUDRATE, 0, 1, 1);
  uart_init(&s_huart3, "usart3", USART3, CH375_WORK_BAUDRATE, 0, 1, 1);
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void log_uart_init(void)
{
  uart_init(&s_huart4, "uart log", UART4, 115200, 1, 0, 1);
  g_uart_log = &s_huart4;
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, green_led_Pin|orange_led_Pin|red_led_Pin|blue_led_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : user_btn_Pin */
  GPIO_InitStruct.Pin = user_btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(user_btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ch375a_int_Pin */
  GPIO_InitStruct.Pin = ch375a_int_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ch375a_int_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ch375b_int_Pin */
  GPIO_InitStruct.Pin = ch375b_int_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ch375b_int_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : green_led_Pin orange_led_Pin red_led_Pin blue_led_Pin */
  GPIO_InitStruct.Pin = green_led_Pin|orange_led_Pin|red_led_Pin|blue_led_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{

}
#endif /* USE_FULL_ASSERT */


int main(void)
{
  int ret = -1;
  USBDevice udev = {0};
  USBHIDDevice hid_dev = {0};
  HIDMouse mouse = {0};
  HIDKeyboard kbd = {0};
  int i = 0;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  
  #ifdef ENABLE_LOG
  log_uart_init();
  #endif

  ch375_init();

  while (1) {
    ret = ch375_host_wait_device_connect(ch375_ctx[i], 1000);
    if (ret == CH375_HST_ERRNO_ERROR) {
      ERROR("ch375 unkown error, need reset");
      continue;
    } else if (ret == CH375_HST_ERRNO_TIMEOUT) {
      ERROR("ch375 wait device connecetd timeout");
      continue;
    }
    INFO("usb device connected");

    ret = ch375_host_udev_open(ch375_ctx[i], &udev);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
      ERROR("ch375 udev init failed, ret=%d", ret);
      continue;
    }
    INFO("udev init success");

    ret = usbhid_open(&udev, 0, &hid_dev);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("usbhid open failed(pvid=%04X:%04X, interface=%d), ret=%d",
        udev.vid, udev.pid, 0, ret);
      ch375_host_udev_close(&udev);
      continue;
    }
    INFO("usbhid init success, hid_type=%d", hid_dev.hid_type);

    if (hid_dev.hid_type == USBHID_TYPE_MOUSE) {
      ret = hid_mouse_open(&hid_dev, &mouse);
      if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("HID mouse init failed, ret=%d", ret);
        usbhid_close(&hid_dev);
        ch375_host_udev_close(&udev);
        continue;
      }
      INFO("1hid mouse init success");
      break;
    } else if (hid_dev.hid_type == USBHID_TYPE_KEYBOARD) {
      ret = hid_keyboard_open(&hid_dev, &kbd);
      if (ret != USBHID_ERRNO_SUCCESS) {
        ERROR("HID keybaord init failed, ret=%d", ret);
        usbhid_close(&hid_dev);
        ch375_host_udev_close(&udev);
        continue;
      }
      INFO("hid keyboard init success");
      break;
    } else {
      usbhid_close(&hid_dev);
      ch375_host_udev_close(&udev);
    }
  }

  USBD_COMPOSITE_HID_InterfaceRegister(0, (uint8_t *)hid_dev.hid_desc, hid_dev.raw_hid_report_desc, hid_dev.raw_hid_report_desc_len, 8, 1);
  USBD_COMPOSITE_HID_Init();

  MX_USB_DEVICE_Init();

  if (hid_dev.hid_type == USBHID_TYPE_MOUSE) {
    loop_handle_mosue(&mouse);
  } else {
    loop_handle_keyboard(&kbd);
  }
  ERROR("go out from handle mouse loop");
}