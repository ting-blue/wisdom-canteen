/********************************** (C) COPYRIGHT *******************************
* File Name          : Uart.c
* Author             :
* Version            : V1.1.0
* Date               : 2024/06/01
* Description        : UART4(TX)+UART5(RX) 驱动源文件
*******************************************************************************/
#include <stdio.h>
#include <Uart1.h>

/* ===== 接收状态机 ===== */
typedef enum {
    RX_STATE_IDLE = 0, RX_STATE_HEADER1, RX_STATE_HEADER2,
    RX_STATE_DATA, RX_STATE_FOOTER1,
} RX_State_t;

/* ===== UART4 (TX only, 只发不收) ===== */
volatile uint8_t  UART4_RxFlag = 0;
volatile uint8_t  UART4_RxBuf[UART4_RX_BUF_SIZE];
volatile uint16_t UART4_RxLen = 0;

/* ===== UART5 (RX only, 只收不发) ===== */
volatile uint8_t  UART5_RxFlag = 0;
volatile uint8_t  UART5_RxBuf[UART5_RX_BUF_SIZE];
volatile uint16_t UART5_RxLen = 0;

static RX_State_t rxState = RX_STATE_IDLE;
static uint16_t rxIndex = 0;

volatile uint16_t uart5_bytes = 0;
volatile uint32_t uart5_last_byte = 0;


/* Home页面4个菜名的GBK编码 */
const uint8_t t2_dish_gbk[T2_DISH_LEN] = {0xCE,0xF7,0xBA,0xEC,0xCA,0xC1,0xB3,0xB4,0xBC,0xA6,0xB5,0xB0};
const uint8_t t3_dish_gbk[T3_DISH_LEN] = {0xBA,0xEC,0xCC,0xC7,0xC2,0xF8,0xCD,0xB7};
const uint8_t t4_dish_gbk[T4_DISH_LEN] = {0xB9,0xAC,0xB1,0xA3,0xBC,0xA6,0xB6,0xA1};
const uint8_t t5_dish_gbk[T5_DISH_LEN] = {0xCC,0xC7,0xB4,0xD7,0xC0,0xEF,0xBC,0xB9};

/* 价格表 */
PriceTable_t g_priceTable1 = {0};
PriceTable_t g_priceTable2 = {0};
PriceTable_t g_priceTable3 = {0};

/* ===== UART4初始化 (TX only, 只发不收) ===== */
//void UART4_Init(uint32_t baudrate)
//{
//    GPIO_InitTypeDef  GPIO_InitStructure = {0};
//    USART_InitTypeDef USART_InitStructure = {0};
//
//    RCC_APB1PeriphClockCmd(RCC_USART4EN, ENABLE);
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
//
//    /* TX -> PC10 复用推挽输出 */
//    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
//    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(GPIOC, &GPIO_InitStructure);
//
//    USART_InitStructure.USART_BaudRate            = baudrate;
//    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
//    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
//    USART_InitStructure.USART_Parity              = USART_Parity_No;
//    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
//    USART_InitStructure.USART_Mode                = USART_Mode_Tx;
//    USART_Init(UART4, &USART_InitStructure);
//
//    USART_Cmd(UART4, ENABLE);
//}

/* ===== UART5初始化 (RX only, 只收不发) ===== */
void UART5_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef  NVIC_InitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_USART5EN, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    /* RX -> PD2 浮空输入 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx;
    USART_Init(UART5, &USART_InitStructure);

    /* 使能接收中断 */
    USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);

    /* NVIC 中断优先级配置 */
    NVIC_InitStructure.NVIC_IRQChannel                   = UART5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(UART5, ENABLE);

    rxState     = RX_STATE_IDLE;
    rxIndex     = 0;
    UART5_RxFlag = 0;
    UART5_RxLen  = 0;
}

/* ===== UART5中断 (接收串口屏数据) ===== */
void UART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
//void UART5_IRQHandler(void)
//{
//    if (USART_GetITStatus(UART5, USART_IT_RXNE) != RESET)
//    {
//        uint8_t data = (uint8_t)(USART_ReceiveData(UART5) & 0xFF);
//        switch (rxState)
//        {
//            case RX_STATE_IDLE:
//                if (data == FRAME_HEADER1) rxState = RX_STATE_HEADER1;//检测0x55,0xAA
//                break;
////            case RX_STATE_HEADER1:
////                if (data == FRAME_HEADER2) { rxState = RX_STATE_HEADER2; rxIndex = 0; }
////                else rxState = RX_STATE_IDLE;
////                break;
//
//            case RX_STATE_HEADER1:
//                rxState = RX_STATE_HEADER2;
//                rxIndex = 0;
//                break;
//
//            case RX_STATE_HEADER2:
//            case RX_STATE_DATA:
//                if (data == FRAME_FOOTER1) rxState = RX_STATE_FOOTER1;
//                else { rxState = RX_STATE_DATA; if (rxIndex < UART5_RX_BUF_SIZE) UART5_RxBuf[rxIndex++] = data; }
//                break;
//            case RX_STATE_FOOTER1:
//                if (data == FRAME_FOOTER2) { UART5_RxLen = rxIndex; UART5_RxFlag = 1; }
//                rxState = RX_STATE_IDLE;
//                break;
//            default: rxState = RX_STATE_IDLE; break;
//        }
//    }
//}


void UART5_IRQHandler(void)
{
    if (USART_GetITStatus(UART5, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t)(USART_ReceiveData(UART5) & 0xFF);
        if(uart5_bytes < 511)
            UART5_RxBuf[uart5_bytes++] = data;
        uart5_last_byte = GetSysTick();
    }
}



/* ===== 发送函数 (TX走UART4) ===== */
void UART4_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
    USART_SendData(UART4, (uint16_t)data);
}

void UART4_SendString(const char *str)
{
    while (*str != '\0') { UART4_SendByte((uint8_t)*str); str++; }
}

void UART4_SendFrame(const char *cmd)
{
    while (*cmd != '\0') { UART4_SendByte((uint8_t)*cmd); cmd++; }
    while (USART_GetFlagStatus(UART4, USART_FLAG_TC) == RESET);
}


//void UART4_SendFrame(const char *cmd)
//{
//    // 发送指令内容
//    while(*cmd != '\0') {
//        while(USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
//        USART_SendData(UART4, (uint8_t)*cmd);
//        cmd++;
//    }
//    // 发送结束符
//    for(int i = 0; i < 3; i++) {
//        while(USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
//        USART_SendData(UART4, 0xFF);
//    }
//    // ★ 关键：等待所有数据发送完成
//    while(USART_GetFlagStatus(UART4, USART_FLAG_TC) == RESET);
//}

/*********************************************************************
 * @fn      UART4_SendAllDishes
 * @brief   上电发送菜名到Home，然后匹配价格
 */
void UART4_SendAllDishes(void)
{
    Delay_Ms(10);
    UART4_SendFrame("Home.t2.txt=\"西红柿炒鸡蛋\"\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("Home.t3.txt=\"鱼香肉丝\"\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("Home.t4.txt=\"宫保鸡丁\"\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("Home.t5.txt=\"糖醋里脊\"\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("List.x4.val=111\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("List.x5.val=111\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("List.x6.val=122\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("List.x7.val=133\xff\xff\xff");
    Delay_Ms(100);
    UART4_SendFrame("page Home\xff\xff\xff");
    Delay_Ms(100);

    /* 匹配并发送价格到Home.x0~x3 */
    UART4_MatchAndSendHomePrices();
}

/*********************************************************************
 * @fn      UART4_ProcessPriceFrame
 */
void UART4_ProcessPriceFrame(uint8_t *buf, uint16_t len)
{
    uint8_t page_id = buf[0];
    PriceTable_t *table = NULL;
    uint8_t max_items = 0, item, i;
    uint16_t pos, semicolon_pos;

    switch (page_id)
    {
        case 0x11: table = &g_priceTable1; max_items = 6; break;
        case 0x12: table = &g_priceTable2; max_items = 6; break;
        case 0x13: table = &g_priceTable3; max_items = 10; break;
        default: return;
    }

    semicolon_pos = 0;
    for (pos = 1; pos < len; pos++) { if (buf[pos] == 0x3B) { semicolon_pos = pos; break; } }
    if (semicolon_pos == 0) return;

    pos = 1; item = 0;
    while (pos < semicolon_pos && item < max_items)
    {
        uint8_t nl = 0;
        while (pos + nl < semicolon_pos && buf[pos + nl] != 0x2C) nl++;
        for (i = 0; i < nl && i < MAX_DISH_NAME_LEN - 1; i++) table->names[item][i] = buf[pos + i];
        table->names[item][nl] = '\0';
        table->name_len[item] = nl;
        pos += nl;
        if (pos < semicolon_pos && buf[pos] == 0x2C) pos++;
        item++;
    }
    table->dish_count = item;
    table->updated = 1;
}

/*********************************************************************
 * @fn      UART4_MatchAndSendHomePrices
 * @brief   在3个价格表中查找Home.t2~t5菜名，匹配则发价格到Home.x0~x3
 */
void UART4_MatchAndSendHomePrices(void)
{
    uint8_t di, ti, pi, k, match;
    uint16_t price;
    char cmd[64];

    const uint8_t *dishes[4] = {t2_dish_gbk, t3_dish_gbk, t4_dish_gbk, t5_dish_gbk};
    const uint8_t dish_lens[4] = {T2_DISH_LEN, T3_DISH_LEN, T4_DISH_LEN, T5_DISH_LEN};
    PriceTable_t *tables[3] = {&g_priceTable1, &g_priceTable2, &g_priceTable3};

    for (di = 0; di < 4; di++)
    {
        price = 0;
        for (ti = 0; ti < 3; ti++)
        {
            if (!tables[ti]->updated) continue;
            for (pi = 0; pi < tables[ti]->dish_count; pi++)
            {
                if (tables[ti]->name_len[pi] != dish_lens[di]) continue;
                match = 1;
                for (k = 0; k < dish_lens[di]; k++)
                {
                    if (tables[ti]->names[pi][k] != dishes[di][k])
                    { match = 0; break; }
                }
                if (match)
                {
                    price = tables[ti]->prices[pi];
                    ti = 3; break;
                }
            }
        }
        sprintf(cmd, "Home.x%d.val=%d\xff\xff\xff", di, price);
        UART4_SendFrame(cmd);
        Delay_Ms(30);
    }
}
