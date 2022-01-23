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
#include "auto_gun_press.h"

#include "main.h"

typedef struct DeviceIN {
  const char *name;
  USART_TypeDef *uart;
  UART_HandleTypeDef *huart;
  GPIO_TypeDef *gpio_port_int;
  uint16_t gpio_pin_int;

  CH375Context *ch375_ctx;
  USBDevice usbdev;
  USBHIDDevice hiddev;
  HIDMouse mouse;
  HIDKeyboard keyboard;

  uint8_t is_connected;
} DeviceIN;

static UART_HandleTypeDef s_huart2; // ch375a
static UART_HandleTypeDef s_huart3; // ch375b
static UART_HandleTypeDef s_huart4; // log

// CH375 CONFIG
#define CH375_WORK_BAUDRATE 115200
#define CH375_DEFAULT_BAUDRATE 9600
#define CH375_MODULE_NUM 2

static DeviceIN s_arr_devin[CH375_MODULE_NUM] = {0};


static int ch375_func_write_cmd(CH375Context *context, uint8_t cmd)
{
  assert(context);
  DeviceIN *devin = (DeviceIN *)ch375_get_priv(context);
  uint16_t buf = CH375_CMD(cmd);
  uint8_t status;
  
  status = HAL_UART_Transmit(devin->huart, (uint8_t *)&buf, 1, 500);
  if (status != HAL_OK) {
      return CH375_ERROR;
  }

  DEBUG("send cmd: origin data=0x%04X", buf);
  return CH375_SUCCESS;
}

static int ch375_func_write_data(CH375Context *context, uint8_t data)
{
  assert(context);
  DeviceIN *devin = (DeviceIN *)ch375_get_priv(context);
  uint16_t buf = CH375_DATA(data);
  uint8_t status;

  status = HAL_UART_Transmit(devin->huart, (uint8_t *)&buf, 1, 500);
  if (status != HAL_OK) {
      return CH375_ERROR;
  }

  DEBUG("(%s) send cmd: origin data=0x%04X", devin->name, buf);
  return CH375_SUCCESS;
}

static int ch375_func_read_data(CH375Context *context, uint8_t *data)
{
  assert(context);
  DeviceIN *devin = (DeviceIN *)ch375_get_priv(context);
  uint16_t buf = 0;
  uint8_t status;

  status = HAL_UART_Receive(devin->huart, (uint8_t *)&buf, 1, 500);//HAL_MAX_DELAY
  if (status != HAL_OK) {
      ERROR("uart receive failed %d", status);
      return CH375_ERROR;
  }
  DEBUG("(%s) receive data: origin data=0x%04X", devin->name, buf);
  *data = 0xFF & buf;
  return CH375_SUCCESS;
}

// @return 0 not intrrupt, other interrupted
static int ch375_func_query_int(CH375Context *context)
{
  assert(context);
  DeviceIN *devin = (DeviceIN *)ch375_get_priv(context);
  uint8_t val = HAL_GPIO_ReadPin(devin->gpio_port_int, devin->gpio_pin_int);
  return val == 0 ? 1 : 0;
}

static uint8_t s_enable_gun_press = 0;
static uint8_t s_started_gun_press = 0;
static AGPContext *s_agp_ctx = NULL;

#ifdef RECORD_SUPPORT

#define RECORAD_BUFSIZE 20000
static int32_t s_mouse_record_buffer[RECORAD_BUFSIZE];
static uint32_t s_mouse_record_last_tick = 0;
static uint32_t s_mouse_record_idx = 0;
static uint8_t s_enable_mouse_recoard = 0;

static void handle_recoard_gun_press(HIDMouse *dev)
{
  int32_t x, y;
  hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_X, &x, USBHID_NOW);
  hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_X, &y, USBHID_NOW);

  s_mouse_record_buffer[s_mouse_record_idx] = x;
  s_mouse_record_buffer[s_mouse_record_idx + 1] = y;
  s_mouse_record_buffer[s_mouse_record_idx + 2] = (uint32_t)HAL_GetTick() - s_mouse_record_last_tick;
  s_mouse_record_idx += 3;

  s_mouse_record_last_tick =  HAL_GetTick();
}
#endif /* RECORD_SUPPORT */


static int handle_auto_gun_press(HIDMouse *dev, uint8_t fetch_success)
{
  static AGPData data = {0};
  int ret;
  int32_t value;
  
  ret = agp_get_data(s_agp_ctx, &data);
  if (ret < 0) {
    return -1;
  }

  if (fetch_success) {
    hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_X, &value, USBHID_NOW);
    data.x += value;
    hid_mouse_get_orientation(dev, HID_MOUSE_AXIS_Y, &value, USBHID_NOW);    
    data.y += value;
  }

  hid_mouse_set_orientation(dev, HID_MOUSE_AXIS_X, data.x, USBHID_NOW);
  hid_mouse_set_orientation(dev, HID_MOUSE_AXIS_Y, data.y, USBHID_NOW);
  return 0;
}

static int handle_mosue(HIDMouse *dev, uint8_t interface_num)
{
  USBHIDDevice *hiddev = dev->hid_dev;
  uint8_t *report_buf = NULL;
  int ret;
  uint32_t value;
  uint8_t fetch_success = 0;
  uint8_t need_send = 0;

  ret = hid_mouse_fetch_report(dev);
  if (ret == USBHID_ERRNO_SUCCESS) {
    fetch_success++;
    need_send++;
  }

  (void)usbhid_get_report_buffer(hiddev, &report_buf, NULL, USBHID_NOW);

  hid_mouse_get_button(dev, HID_MOUSE_BUTTON_LEFT, &value, USBHID_NOW);
  if (value) {
    if (s_started_gun_press == 0) {
      s_started_gun_press = 1;
      agp_restart(s_agp_ctx);
    }
  } else {
    s_started_gun_press = 0;
  }
  
  if (s_enable_gun_press && s_started_gun_press) {
    #ifdef RECORD_SUPPORT
    if (s_enable_mouse_recoard) {
      // recoard
      handle_recoard_gun_press(dev);
    } else {
      // press
      ret = handle_auto_gun_press(dev, fetch_success);
      if (ret == 0) {
        need_send++;
      }
    }
    #else
    // press
    ret = handle_auto_gun_press(dev, fetch_success);
    if (ret == 0) {
      need_send++;
    }
    #endif /* RECORD_SUPPORT */
  }
  
  if (need_send) {
    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, interface_num, report_buf, hiddev->report_length);
  }
  return USBHID_ERRNO_SUCCESS;
}

#ifdef RECORD_SUPPORT
void print_mouse_recoard()
{
  uint32_t i;

  for (i = 0; i < s_mouse_record_idx; i+=3) {
    printf2("%ld, %ld, %lu\r\n",
      s_mouse_record_buffer[i],
      s_mouse_record_buffer[i+1],
      s_mouse_record_buffer[i+2]);
  }
}
#endif /* RECORD_SUPPORT */

static int handle_keyboard(HIDKeyboard *dev, uint8_t interface_num)
{
  USBHIDDevice *hiddev = dev->hid_dev;
  uint8_t *report_buf = NULL;
  int ret;
  uint32_t value;

  ret = hid_keyboard_fetch_report(dev);
  if (ret != USBHID_ERRNO_SUCCESS) {
    // ERROR("fetch report failed, ret=%d", ret); // TOTO
    return ret;
  }
  (void)usbhid_get_report_buffer(hiddev, &report_buf, NULL, 0);

  #ifdef RECORD_SUPPORT
  // check enable/disable gun press data record
  hid_keyboard_get_key(dev, HID_KBD_LETTER('r'), &value, USBHID_NOW);
  if (value) {
    s_enable_mouse_recoard = 1;
    s_mouse_record_last_tick = HAL_GetTick();
  } else {
    if (s_enable_mouse_recoard) {
      print_mouse_recoard();
    }
    s_enable_mouse_recoard = 0;
    s_mouse_record_idx = 0;
  }
  #endif /* RECORD_SUPPORT */

  // check enable/disable Auto gun press
  hid_keyboard_get_key(dev, HID_KBD_NUMBER(1), &value, USBHID_NOW);
  if (value) {
    s_enable_gun_press = 1;
  }
  hid_keyboard_get_key(dev, HID_KBD_NUMBER(2), &value, USBHID_NOW);
  if (value) {
    s_enable_gun_press = 0;
  }
  hid_keyboard_get_key(dev, HID_KBD_NUMBER(3), &value, USBHID_NOW);
  if (value) {
    s_enable_gun_press = 0;
  }
  hid_keyboard_get_key(dev, HID_KBD_NUMBER(4), &value, USBHID_NOW);
  if (value) {
    s_enable_gun_press = 0;
  }

  hid_keyboard_get_key(dev, HID_KBD_LETTER('n'), &value, USBHID_NOW);
  if (value) {
    agp_set_collect(s_agp_ctx, AGP_COLLECT_IDX_NONE);
  }
  hid_keyboard_get_key(dev, HID_KBD_LETTER('k'), &value, USBHID_NOW);
  if (value) {
    agp_set_collect(s_agp_ctx, AGP_COLLECT_IDX_AK);
  }
  
  USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, interface_num, report_buf, hiddev->report_length);
  return USBHID_ERRNO_SUCCESS;
}

static int loop_handle_devin()
{
  int i;
  int ret;

  ret = agp_open(&s_agp_ctx);
  if (ret < 0) {
    ERROR("agp open failed");
    return -1;
  }

  while (1) {

    for (i = 0; i <  CH375_MODULE_NUM; i++) {
      DeviceIN *devin = &s_arr_devin[i];

      ret = USBHID_ERRNO_SUCCESS; // TODO: remove this

      if (devin->hiddev.hid_type == USBHID_TYPE_MOUSE) {
        ret = handle_mosue(&devin->mouse, i);
      }
      if (devin->hiddev.hid_type == USBHID_TYPE_KEYBOARD) {
        ret = handle_keyboard(&devin->keyboard, (uint8_t)i);
      }

      if (ret == USBHID_ERRNO_NO_DEV) {
        ERROR("(%s) DeviceIN disconnected", devin->name);
        goto out;
      } else if (ret != USBHID_ERRNO_SUCCESS) {
        // ERROR("(%s) DeviceIN handle failed, ret=%d", devin->name, ret); // TOTO
        // TODO: retry max
      }
    }

  }
out:
  agp_close(s_agp_ctx);
  s_agp_ctx = NULL;
  return -1;
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

void devin_init(void)
{
  int i;
  int ret;

  s_arr_devin[0].uart = USART2;
  s_arr_devin[0].name = "CH375A";
  s_arr_devin[0].huart = &s_huart2;
  s_arr_devin[0].gpio_port_int = CH375A_INT_PORT;
  s_arr_devin[0].gpio_pin_int = CH375A_INT_PIN;

  s_arr_devin[1].uart = USART3;
  s_arr_devin[1].name = "CH375B";
  s_arr_devin[1].huart = &s_huart3;
  s_arr_devin[1].gpio_port_int = CH375B_INT_PORT;
  s_arr_devin[1].gpio_pin_int = CH375B_INT_PIN;
  
  
  // UART Init
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    uart_init(s_arr_devin[i].huart, s_arr_devin[i].name,
      s_arr_devin[i].uart, CH375_DEFAULT_BAUDRATE, 0, 1, 1);
  }

  // Init Context
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ret = ch375_open_context(&s_arr_devin[i].ch375_ctx,
            ch375_func_write_cmd,
            ch375_func_write_data,
            ch375_func_read_data,
            ch375_func_query_int,
            &s_arr_devin[i]);
    if (ret != CH375_SUCCESS) {
      ERROR("open ch375(%d) context failed, ret=%d", i, ret);
      Error_Handler();
    }
  }
  // CH375 USB Host Init
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ret = ch375_host_init(s_arr_devin[i].ch375_ctx, CH375_WORK_BAUDRATE);
    if (ret != CH375_HST_ERRNO_SUCCESS) {
      ERROR("ch375(%d) init host failed, %d", i, ret);
      Error_Handler();
    }
  }
  // reinit uart to work baudrate
  for (i = 0; i < CH375_MODULE_NUM; i++) {
    uart_deinit(s_arr_devin[i].huart, s_arr_devin[i].name);
    uart_init(s_arr_devin[i].huart, s_arr_devin[i].name, s_arr_devin[i].uart, CH375_WORK_BAUDRATE, 0, 1, 1);
  }
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

  /*Configure GPIO pin : CH375A_INT_PIN */
  GPIO_InitStruct.Pin = CH375A_INT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(CH375A_INT_PORT, &GPIO_InitStruct);

  /*Configure GPIO pin : CH375B_INT_PIN */
  GPIO_InitStruct.Pin = CH375B_INT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(CH375B_INT_PORT, &GPIO_InitStruct);

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

static void open_device_out()
{
  uint8_t status;
  uint8_t i;

  for (i = 0; i < CH375_MODULE_NUM; i++) {
    DeviceIN *devin = &s_arr_devin[i];
    USBHIDDevice *hiddev = &devin->hiddev;
    // TODO: get actual maxpack and interval
    status = USBD_COMPOSITE_HID_InterfaceRegister(i,
      (uint8_t *)hiddev->hid_desc, hiddev->raw_hid_report_desc, hiddev->raw_hid_report_desc_len, 8, 1);
    if (status != HAL_OK) {
      ERROR("DeviceOUT interface register failed, status=0x%02X", status);
      assert(status == HAL_OK);
    }
  }

  status = USBD_COMPOSITE_HID_Init();
  if (status != HAL_OK) {
    ERROR("DeviceOUT init failed, status=0x%02X", status);
    assert(status == HAL_OK);
  }
}

static int open_device_in(DeviceIN *devin)
{
  assert(devin);

  USBDevice *udev = &devin->usbdev;
  USBHIDDevice *hiddev = &devin->hiddev;
  HIDMouse *mouse = &devin->mouse;
  HIDKeyboard *keyboard = &devin->keyboard;
  int ret;

  ret = ch375_host_udev_open(devin->ch375_ctx, udev);
  if (ret != CH375_HST_ERRNO_SUCCESS) {
    ERROR("(%s) ch375 udev init failed, ret=%d", devin->name, ret);
    return -1;
  }
  INFO("(%s) usb dev init success", devin->name);

  // TODO: actual interface number maybe not zero
  ret = usbhid_open(udev, 0, hiddev);
  if (ret != USBHID_ERRNO_SUCCESS) {
    ERROR("(%s) usbhid open failed(pvid=%04X:%04X, interface=%d), ret=%d",
      devin->name, udev->vid, udev->pid, 0, ret);
    goto free_udev;
  }
  INFO("(%s) usbhid init success, hid_type=%d", devin->name, hiddev->hid_type);

  // open HID class device
  if (hiddev->hid_type == USBHID_TYPE_MOUSE) {
    ret = hid_mouse_open(hiddev, mouse);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("(%s) HID mouse init failed, ret=%d", devin->name, ret);
      goto free_hid;
    }
    INFO("(%s) hid mouse init success", devin->name);

  } else if (hiddev->hid_type == USBHID_TYPE_KEYBOARD) {
    ret = hid_keyboard_open(hiddev, keyboard);
    if (ret != USBHID_ERRNO_SUCCESS) {
      ERROR("(%s) HID keybaord init failed, ret=%d", devin->name, ret);
      goto free_hid;
    }
    INFO("(%s) hid keyboard init success", devin->name);

  } else {
    ERROR("(%s) hid type is invalied 0x%02X", devin->name, hiddev->hid_type);
    goto free_hid;
  }

  return 0;
free_hid:
  usbhid_close(hiddev);
  hiddev = NULL;
free_udev:
  ch375_host_udev_close(udev);
  udev = NULL;
  return -1;
}

static int open_all_device_in()
{
  int i;
  int ret;

  for (i = 0; i < CH375_MODULE_NUM; i++) {
    ret = open_device_in(&s_arr_devin[i]);
    if (ret < 0) {
      ERROR("open DeviceIN %s failed", s_arr_devin[i].name);
      return -1;
    }
  }
  INFO("all DeviceIN open success");
  return 0;
}

static void wait_all_in_device_connect()
{
  uint8_t not_conn;
  int i;
  int ret;
  
  while (1) {
    not_conn = 0;

    for (i = 0; i < CH375_MODULE_NUM; i++) {
      ret = ch375_host_wait_device_connect(s_arr_devin[i].ch375_ctx, 500);
      if (ret == CH375_HST_ERRNO_ERROR) {
        ERROR("ch375 unkown error, need reset");
        not_conn = 1;
        continue;
      } else if (ret == CH375_HST_ERRNO_TIMEOUT) {
        ERROR("ch375 wait device(%d) connecetd timeout", i);
        not_conn = 1;
        continue;
      }
      INFO("usb device(%d) connected", i);
    }

    if (not_conn == 0) {
      break;
    }
  }
}

int main(void)
{
  int ret = -1;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  
  #ifdef ENABLE_LOG
  log_uart_init();
  #endif

  devin_init();

  while (1) {
    INFO("start wait all DeviceIN connnect");
    wait_all_in_device_connect();

    INFO("start to open all DeviceIN");
    ret = open_all_device_in();
    if (ret < 0) {
      ERROR("open all DeviceIN failed");
      continue;
    }

    // clone hid device report descriptor
    INFO("open device out");
    open_device_out();

    // Init USB DeviceOUT
    MX_USB_DEVICE_Init();

    // Loop Handle DeviceIN
    INFO("start loop handle devin");
    loop_handle_devin();
  }

  return 0;
}