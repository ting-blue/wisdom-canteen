#include "debug.h"
#include "uart.h"

void Usart6_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 1. 开启GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 【关键修正】开启UART6时钟 - 使用正确的寄存器名和宏
    // CH32V307的时钟使能寄存器是 APB2PCENR
    // UART6的时钟使能位是 RCC_USART6EN (第6位)
    RCC->APB2PCENR |= RCC_USART6EN;

    // 3. 配置GPIO为复用功能
    // PB8 -> UART6_TX, PB9 -> UART6_RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 复用推挽
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 4. 配置UART6参数 (BY8301-16P: 9600, 8N1)
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART6, &USART_InitStructure);

    // 5. 使能UART6
    USART_Cmd(UART6, ENABLE);
}

void audio_init()
{
    Usart6_Init();
}

void audio_play(u8 num)
{
    // BY8301-16P命令格式：起始码 + 长度 + 操作码 + 参数 + 校验和 + 结束码
    // 0x7E: 起始码, 0x05: 数据长度, 0x41: 选曲播放指令
    // 0x00: 高位地址, num: 曲目号(0-299), 校验和: 异或校验, 0xEF: 结束码
    u8 string[] = {0x7E, 0x05, 0x41, 0x00, num, 0x05 ^ 0x41 ^ 0x00 ^ num, 0xEF};
    u8 i;
    for (i = 0; i < 7; i++)
    {
        USART_SendData(UART6, string[i]);
        while(USART_GetFlagStatus(UART6, USART_FLAG_TC) == 0);
    }
}

// BUSY引脚初始化 (PC2)
void BusyPin_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// 检测模块是否忙碌
// 返回值: 1=忙碌(播放中), 0=空闲
u8 IsAudioBusy(void)
{
    return GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2);
}



