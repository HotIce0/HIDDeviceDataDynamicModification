// #ifndef __LOG_H
// #define __LOG_H

#include "stm32f4xx_hal.h"

#ifdef ENABLE_LOG

    #define INFO(fmt, args...)  printf2("[ INFO]<%s:%d:%s> " fmt "\n\r", __FILE__, __LINE__, __func__, ## args)
    #define ERROR(fmt, args...) printf2("[ERROR]<%s:%d:%s> " fmt "\n\r", __FILE__, __LINE__, __func__, ## args)
    #define WARN(fmt, args...)  printf2("[ WARN]<%s:%d:%s> " fmt "\n\r", __FILE__, __LINE__, __func__, ## args)

    #ifdef ENABLE_DEBUG
        #define DEBUG(fmt, args...) printf2("[DEBUG]<%s:%d:%s> " fmt "\n\r", __FILE__, __LINE__, __func__, ## args)
    #else
        #define DEBUG(fmt, args...)
    #endif /* ENABLE_DEBUG */

#else

    #define INFO(fmt, args...)
    #define ERROR(fmt, args...)
    #define WARN(fmt, args...)
    #define DEBUG(fmt, args...)

#endif /* ENABLE_LOG */

extern UART_HandleTypeDef *g_uart_log;

void printf2(char *fmt, ...);

void hex_dump(uint8_t *buf, int len);
// #endif /* __LOG_H */