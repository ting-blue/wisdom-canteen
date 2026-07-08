#ifndef __ADS1234_H
#define __ADS1234_H

#include "ch32v30x.h"

// ADS1234 引脚定义
#define ADS_SCLK_PORT       GPIOA
#define ADS_SCLK_PIN        GPIO_Pin_0
#define ADS_DOUT_PORT       GPIOA
#define ADS_DOUT_PIN        GPIO_Pin_1

// PDWN 使用 PB0
#define ADS_PDWN_PORT       GPIOB
#define ADS_PDWN_PIN        GPIO_Pin_1

// 通道选择引脚
#define ADS_A0_PORT         GPIOB
#define ADS_A0_PIN          GPIO_Pin_2
#define ADS_A1_PORT         GPIOB
#define ADS_A1_PIN          GPIO_Pin_5

// ADS1234 参数配置
#define ADS_FULL_SCALE      8388607UL
#define ADS_REF_VOLTAGE     5.0f
#define ADS_GAIN            128.0f

// 通道定义
typedef enum
{
    CHANNEL_1 = 0,
    CHANNEL_2 = 1,
    CHANNEL_3 = 2,
    CHANNEL_4 = 3
} ADS1234_Channel_t;

// 引脚控制宏
#define ADS_SCLK_LOW()      GPIO_ResetBits(ADS_SCLK_PORT, ADS_SCLK_PIN)
#define ADS_SCLK_HIGH()     GPIO_SetBits(ADS_SCLK_PORT, ADS_SCLK_PIN)
#define ADS_DOUT_READ()     GPIO_ReadInputDataBit(ADS_DOUT_PORT, ADS_DOUT_PIN)
#define ADS_PDWN_LOW()      GPIO_ResetBits(ADS_PDWN_PORT, ADS_PDWN_PIN)
#define ADS_PDWN_HIGH()     GPIO_SetBits(ADS_PDWN_PORT, ADS_PDWN_PIN)

// 通道选择宏
#define ADS_SET_CHANNEL_1() do { GPIO_ResetBits(ADS_A0_PORT, ADS_A0_PIN); GPIO_ResetBits(ADS_A1_PORT, ADS_A1_PIN); } while(0)
#define ADS_SET_CHANNEL_2() do { GPIO_SetBits(ADS_A0_PORT, ADS_A0_PIN); GPIO_ResetBits(ADS_A1_PORT, ADS_A1_PIN); } while(0)
#define ADS_SET_CHANNEL_3() do { GPIO_ResetBits(ADS_A0_PORT, ADS_A0_PIN); GPIO_SetBits(ADS_A1_PORT, ADS_A1_PIN); } while(0)
#define ADS_SET_CHANNEL_4() do { GPIO_SetBits(ADS_A0_PORT, ADS_A0_PIN); GPIO_SetBits(ADS_A1_PORT, ADS_A1_PIN); } while(0)

// 外部变量声明（供 main.c 使用）
extern float g_scale_factor[4];

// 函数声明
void ADS1234_GPIO_Config(void);
void ADS1234_Init(void);
void ADS1234_SetChannel(ADS1234_Channel_t channel);
int32_t ADS1234_ReadData(ADS1234_Channel_t channel);
float ADS1234_ToVoltage(int32_t adc);
float ADS1234_ToWeight(ADS1234_Channel_t channel, int32_t adc);
void ADS1234_PrintData(int32_t adc, float vol);
void ADS1234_AutoZero(void);

// 校准函数
void ADS1234_CalibrateZero(ADS1234_Channel_t channel);
void ADS1234_CalibrateScale(ADS1234_Channel_t channel, float weight_gram);
void ADS1234_PrintCalibration(ADS1234_Channel_t channel);
void ADS1234_SetCalibration(ADS1234_Channel_t channel, int32_t zero_adc, float scale_factor);

#endif /* __ADS1234_H */
