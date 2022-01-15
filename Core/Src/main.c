/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <assert.h>
#define ENABLE_LOG
// #define ENABLE_DEBUG
#include "log.h"

#include "usbd_customhid.h"
#include "ch375_usbhost.h"
#include "hid/usbhid.h"
#include "hid/hid_mouse.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
enum MOUSE_BUTTONS
{
  MOUSE_BTN_LEFT = 1,
  MOUSE_BTN_RIGHT = 2,
  MOUSE_BTN_MIDDLE = 4
};

typedef struct mouse_report_t
{
  uint8_t buttons; //[0, 3] 1~3 btn, [4, 7] reserve
  int8_t x;        // -127 ~ 127
  int8_t y;        // -127 ~ 127
  int8_t wheel;    // -127 ~ 127
} mouse_report_t;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

typedef struct CH375Priv {
  UART_HandleTypeDef *huart;
  GPIO_TypeDef *gpio_int;
  uint16_t gpio_pin_int;
} CH375Priv;

void delay_us(uint16_t time)
{    
   uint16_t i=0;  
   while(time--)
   {
      i=10;  //自己定义
      while(i--) ;    
   }
}

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
  delay_us(2);
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
  delay_us(1);
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
  uint8_t val = HAL_GPIO_ReadPin(priv->gpio_int, priv->gpio_pin_int);
  return val == 0 ? 1 : 0;
}

#define REPORT_BUFSIZE 20

static void loop_handle_mosue(HIDMouse *dev)
{
  int32_t x;
  int32_t y;
  int ret;
  int i;

  while (1) {
    ret = hid_mouse_fetch_report(dev);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("fetch report failed, ret=%d", ret);
      if (ret != USBHID_ERRNO_NO_DEV) {
        return;
      }
    }
    // Button click check
    // hid_mouse_set_button(dev, HID_MOUSE_BUTTON_LEFT, 1, 0);
    for (i = 0; i < dev->button.count; i++) {
      uint32_t is_pressed;
      (void)hid_mouse_get_button(dev, i, &is_pressed, 0);
      if (is_pressed) {
        INFO("button %d is pressed", i);
      }
    }
    // move print
    // hid_mouse_set_orientation(dev, HID_MOUSE_AXIS_X, 0x7FFF, 0);
    // hid_mouse_set_orientation(dev, HID_MOUSE_AXIS_Y, 0x8001, 0);
    (void)hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_X, &x, 0);
    (void)hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_Y, &y, 0);
    if (!(x == 0 && y == 0)) {
      INFO("mouse move(%d,%d)", x, y);
    }
  }

  // never
  return;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  int ret = -1;
  mouse_report_t mouse_report = {0};
  CH375Priv priv = {0};
  CH375Context *ctx = NULL;;
  USBDevice udev = {0};
  USBHIDDevice hid_dev = {0};
  
  priv.huart = &huart3;
  priv.gpio_int = usb1_int_GPIO_Port;
  priv.gpio_pin_int = usb1_int_Pin;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  ret = ch375_open_context(&ctx,
          ch375_func_write_cmd,
          ch375_func_write_data,
          ch375_func_read_data,
          ch375_func_query_int,
          &priv);
  if (ret != CH375_SUCCESS) {
    ERROR("open ch375 context failed, ret=%d", ret);
    return -1;
  }

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */

#ifdef ENABLE_LOG
  g_uart_log = &huart4;
#endif
  
  ret = ch375_host_init(ctx);
  if (ret != CH375_HST_ERRNO_SUCCESS) {
    ERROR("ch375 init host on USART3 failed, %d\n", ret);
  }

  ret = ch375_host_wait_device_connect(ctx, 5000);
  if (ret == CH375_HST_ERRNO_ERROR) {
    ERROR("ch375 unkown error, need reset");
    goto loop;
  } else if (ret == CH375_HST_ERRNO_TIMEOUT) {
    ERROR("ch375 wait device connecetd timeout");
    goto loop;
  }
  INFO("usb device connected");

  ret = ch375_host_udev_open(ctx, &udev);
  if (ret != CH375_HST_ERRNO_SUCCESS) {
    ERROR("ch375 udev init failed, ret=%d", ret);
    goto loop;
  }
  INFO("udev init success");

  ret = usbhid_open(&udev, 0, &hid_dev);
  if (ret != USBHID_ERRNO_SUCCESS) {
    ERROR("usbhid open failed(pvid=%04X:%04X, interface=%d), ret=%d",
      udev.vid, udev.pid, 0, ret);
    goto loop;
  }
  INFO("usbhid init success, hid_type=%d", hid_dev.hid_type);

  if (hid_dev.hid_type == USBHID_TYPE_MOUSE) {
    HIDMouse mouse = {0};
    ret = hid_mouse_open(&hid_dev, &mouse);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("HID mouse init failed, ret=%d", ret);
      goto loop;
    }
    INFO("hid mouse init success");

    loop_handle_mosue(&mouse);
    ERROR("go out from handle mouse loop");
  } else if (hid_dev.hid_type == USBHID_TYPE_KEYBOARD) {
    
  } else {
    goto loop;
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
loop:
  while (1)
  {
    uint8_t is_user_btn_down = HAL_GPIO_ReadPin(user_btn_GPIO_Port, user_btn_Pin);

    if (is_user_btn_down)
    {
      mouse_report.buttons |= MOUSE_BTN_LEFT;
    }
    else
    {
      mouse_report.buttons &= ~MOUSE_BTN_LEFT;
    }

    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, (uint8_t *)&mouse_report, sizeof(mouse_report));
    HAL_GPIO_WritePin(orange_led_GPIO_Port, orange_led_Pin, is_user_btn_down);
    HAL_Delay(100);
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  ch375_close_context(ctx);
  ctx = NULL;
  /* USER CODE END 3 */
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
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_9B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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

  /*Configure GPIO pin : usb1_int_Pin */
  GPIO_InitStruct.Pin = usb1_int_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(usb1_int_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : green_led_Pin orange_led_Pin red_led_Pin blue_led_Pin */
  GPIO_InitStruct.Pin = green_led_Pin|orange_led_Pin|red_led_Pin|blue_led_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
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
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
