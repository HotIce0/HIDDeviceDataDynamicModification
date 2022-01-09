#include <stdio.h>
#include <stdarg.h>

#include "log.h"
// #ifdef ENABLE_LOG

UART_HandleTypeDef *g_uart_log = NULL;


#define LOG_BUFSIZE 512

void printf2(char *fmt, ...)
{
    static char buffer[LOG_BUFSIZE];
    uint16_t i = 0;

    if (g_uart_log == NULL) {
        return;
    }

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vsnprintf(buffer, LOG_BUFSIZE, fmt, arg_ptr);
    while (i <= LOG_BUFSIZE && buffer[i]) {
        HAL_UART_Transmit(g_uart_log, (uint8_t *)&buffer[i], 1, HAL_MAX_DELAY);
        i++;
    }
    va_end(arg_ptr);
    HAL_Delay(100); // wait serial assi to next line
}

// #endif