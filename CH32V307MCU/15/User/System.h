#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "ch32v30x.h"

void SystemClock_Config(void);
//void Delay_Ms(uint32_t ms);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);
uint32_t GetSysTick(void);

#endif /* __SYSTEM_H */
