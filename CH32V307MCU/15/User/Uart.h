#ifndef __UART_H
#define __UART_H

#include "ch32v30x.h"
#include <stdio.h>

void USART1_GPIO_Config(void);
void USART1_Config(uint32_t baudrate);
int fputc(int ch, FILE *f);
void UART4_Init(uint32_t baudrate);
void UART8_TX_Init(uint32_t baudrate);
void UART8_SendByte(uint8_t data);
void UART8_SendBytes(uint8_t *data, uint16_t len);


#endif /* __UART_H */
