#include "Uart.h"

void USART1_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void USART1_Config(uint32_t baudrate)
{
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

//int fputc(int ch, FILE *f)
//{
//    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
//    USART_SendData(USART1, (uint8_t)ch);
//    return ch;
//}


// ���� printf �ض���
int fputc(int ch, FILE *f)
{
    // �ȴ����ͻ�����Ϊ��
    while(USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
    USART_SendData(UART4, (uint8_t)ch);
    return ch;
}

void UART4_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(UART4, &USART_InitStructure);

    USART_Cmd(UART4, ENABLE);

    printf("[INIT] UART4��ʼ����� (PC10/TX, PC11/RX, 115200)\r\n");
}



/**
  * @brief  UART8 初始化函数（适用于沁恒CH32V307）
  * @param  baudrate: 串口通信波特率，如 115200、9600 等
  * @retval None
  */
void UART8_TX_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};  // GPIO配置结构体，初始化为0
    USART_InitTypeDef USART_InitStructure = {0}; // USART配置结构体，初始化为0
    NVIC_InitTypeDef  NVIC_InitStructure = {0};  // NVIC中断配置结构体，初始化为0

    /* 1. 使能外设时钟 */
    // UART8挂载在APB1总线上，使能其时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART8, ENABLE);
    // GPIOC挂载在APB2总线上，使能其时钟（UART8的TX/RX使用GPIOC引脚）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /* 2. 配置RX引脚（接收引脚）- PC5 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;          // 选择PC5引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;    // 设置GPIO速度为10MHz
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING; // RX引脚配置为浮空输入模式
    GPIO_Init(GPIOC, &GPIO_InitStructure);               // 将配置写入GPIOC

    /* 3. 配置TX引脚（发送引脚）- PC4 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;          // 选择PC4引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;    // 设置GPIO速度为10MHz
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;     // TX引脚配置为复用推挽输出模式
    GPIO_Init(GPIOC, &GPIO_InitStructure);               // 将配置写入GPIOC

    /* 4. 配置UART8通信参数 */
    USART_InitStructure.USART_BaudRate            = baudrate;           // 设置波特率
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b; // 数据位：8位
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;    // 停止位：1位
    USART_InitStructure.USART_Parity              = USART_Parity_No;     // 校验位：无校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;  // 使能接收和发送模式
    USART_Init(UART8, &USART_InitStructure);        // 将配置写入UART8

    /* 5. 使能UART8接收中断（RXNE中断） */
    // RXNE：接收数据寄存器非空中断，当接收到数据时触发
    USART_ITConfig(UART8, USART_IT_RXNE, ENABLE);

    /* 6. 配置UART8中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel                   = UART8_IRQn;        // 选择UART8中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;                 // 抢占优先级：0（最高）
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;                 // 子优先级：0（最高）
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;            // 使能该中断通道
    NVIC_Init(&NVIC_InitStructure);                       // 将配置写入NVIC

    /* 7. 使能UART8外设 */
    USART_Cmd(UART8, ENABLE);  // 启动UART8，开始工作
}

/**
  * @brief  UART8 发送单字节数据（阻塞方式）
  * @param  data: 要发送的数据（8位）
  * @retval None
  * @note   函数会一直等待直到发送缓冲区为空才发送
  */
void UART8_SendByte(uint8_t data)
{
    // 等待发送数据寄存器为空（TXE标志位为1）
    // TXE = 1 表示上一个数据已发送完毕，可以发送新数据
    while (USART_GetFlagStatus(UART8, USART_FLAG_TXE) == RESET);

    // 发送数据（将8位数据写入发送数据寄存器）
    USART_SendData(UART8, (uint16_t)data);
}

/**
  * @brief  UART8 发送多字节数据（阻塞方式）
  * @param  data: 要发送的数据缓冲区指针
  * @param  len:  要发送的数据长度（字节数）
  * @retval None
  * @note   循环调用单字节发送函数，适合发送字符串或数组
  */
void UART8_SendBytes(uint8_t *data, uint16_t len)
{
    // 循环发送每个字节
    for (uint16_t i = 0; i < len; i++) {
        // 等待发送数据寄存器为空
        while (USART_GetFlagStatus(UART8, USART_FLAG_TXE) == RESET);
        // 发送当前字节
        USART_SendData(UART8, (uint16_t)data[i]);
    }
}


