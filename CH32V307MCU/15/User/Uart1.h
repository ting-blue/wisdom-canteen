#ifndef __UART_H
#define __UART_H

#include "debug.h"

#define UART4_BAUDRATE          115200
#define UART4_RX_BUF_SIZE       512
#define UART5_RX_BUF_SIZE       512

#define FRAME_HEADER1           0x55
#define FRAME_HEADER2           0xAA
#define FRAME_FOOTER1           0x0D
#define FRAME_FOOTER2           0x0A

#define PRICE_PAGE_1            0x11
#define PRICE_PAGE_2            0x12
#define PRICE_PAGE_3            0x13

#define PRICE_P1_X0            0x31
#define PRICE_P1_X1            0x32
#define PRICE_P1_X2            0x33
#define PRICE_P1_X3            0x34
#define PRICE_P1_X4            0x35
#define PRICE_P1_X5            0x36

#define PRICE_P2_X0            0x41
#define PRICE_P2_X1            0x42
#define PRICE_P2_X2            0x43
#define PRICE_P2_X3            0x44
#define PRICE_P2_X4            0x45
#define PRICE_P2_X5            0x46

#define PRICE_P3_X1            0x51
#define PRICE_P3_X2            0x52
#define PRICE_P3_X3            0x53
#define PRICE_P3_X4            0x54
#define PRICE_P3_X5            0x55
#define PRICE_P3_X6            0x56
#define PRICE_P3_X7            0x57
#define PRICE_P3_X8            0x58
#define PRICE_P3_X9            0x59
#define PRICE_P3_X10           0x5A

#define MAX_DISH_PER_PAGE       10
#define MAX_DISH_NAME_LEN       24

typedef struct {
    uint8_t  dish_count;
    uint8_t  names[MAX_DISH_PER_PAGE][MAX_DISH_NAME_LEN];
    uint8_t  name_len[MAX_DISH_PER_PAGE];
    uint16_t prices[MAX_DISH_PER_PAGE];
    uint8_t  updated;
} PriceTable_t;

#define T2_DISH_LEN             12
#define T3_DISH_LEN             8
#define T4_DISH_LEN             8
#define T5_DISH_LEN             8

extern const uint8_t t2_dish_gbk[T2_DISH_LEN];
extern const uint8_t t3_dish_gbk[T3_DISH_LEN];
extern const uint8_t t4_dish_gbk[T4_DISH_LEN];
extern const uint8_t t5_dish_gbk[T5_DISH_LEN];

extern volatile uint8_t  UART4_RxFlag;
extern volatile uint8_t  UART4_RxBuf[UART4_RX_BUF_SIZE];
extern volatile uint16_t UART4_RxLen;

extern volatile uint8_t  UART5_RxFlag;
extern volatile uint8_t  UART5_RxBuf[UART5_RX_BUF_SIZE];
extern volatile uint16_t UART5_RxLen;

extern PriceTable_t g_priceTable1;
extern PriceTable_t g_priceTable2;
extern PriceTable_t g_priceTable3;

void UART4_Init(uint32_t baudrate);
void UART5_Init(uint32_t baudrate);
void UART4_SendByte(uint8_t data);
void UART4_SendString(const char *str);
void UART4_SendFrame(const char *cmd);
void UART4_SendAllDishes(void);
void UART4_ProcessPriceFrame(uint8_t *buf, uint16_t len);
void UART4_MatchAndSendHomePrices(void);
void UART5_IRQHandler(void);

#endif