#include "System.h"

volatile uint32_t g_sys_tick = 0;

void SystemClock_Config(void)
{
    RCC_HSEConfig(RCC_HSE_ON);
    while(RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);

    RCC->CFGR0 &= ~0x003F0000;
    RCC->CFGR0 |= 0x00280000;
    RCC->CFGR0 |= 0x00010000;

    RCC_PLLCmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK2Config(RCC_HCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div2);

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while(RCC_GetSYSCLKSource() != 0x08);

    SystemCoreClockUpdate();
}

//void Delay_Ms(uint32_t ms)
//{
//    uint32_t i, j;
//    for(i = 0; i < ms; i++)
//    {
//        for(j = 0; j < 10000; j++)
//        {
//            __NOP();
//        }
//    }
//}
void delay_ms(uint32_t ms)
{
    uint32_t i, j;
    uint32_t cycles = SystemCoreClock / 4000;
    for(i = 0; i < ms; i++)
        for(j = 0; j < cycles; j++)
            __NOP();
}

void delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    if(cycles == 0) cycles = 1;
    while(cycles--)
    {
        __NOP();
    }
}

uint32_t GetSysTick(void)
{
    return g_sys_tick;
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    SysTick->SR = 0;
    g_sys_tick++;
}
