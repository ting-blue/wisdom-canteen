#include "ADS1234.h"
#include "System.h"
#include "Uart.h"
#include <stdio.h>

extern void USART1_SendString(char *str);

// 每个通道的校准参数
static int32_t g_zero_adc[4] = {0};
float g_scale_factor[4] = {0.0f};
static float g_known_weight[4] = {0.0f};
static int32_t g_known_adc[4] = {0};
static uint8_t g_is_calibrated[4] = {0};

//================================================
// 函数: ADS1234_GPIO_Config
//================================================
void ADS1234_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // PA0 - SCLK
    GPIO_InitStructure.GPIO_Pin = ADS_SCLK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ADS_SCLK_PORT, &GPIO_InitStructure);
    ADS_SCLK_LOW();

    // PA1 - DOUT
    GPIO_InitStructure.GPIO_Pin = ADS_DOUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(ADS_DOUT_PORT, &GPIO_InitStructure);

    // PB0 - PDWN
    GPIO_InitStructure.GPIO_Pin = ADS_PDWN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ADS_PDWN_PORT, &GPIO_InitStructure);
    ADS_PDWN_HIGH();

    // PB2 - A0
    GPIO_InitStructure.GPIO_Pin = ADS_A0_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ADS_A0_PORT, &GPIO_InitStructure);

    // PB5 - A1
    GPIO_InitStructure.GPIO_Pin = ADS_A1_PIN;
    GPIO_Init(ADS_A1_PORT, &GPIO_InitStructure);

    // 设为通道1
    printf("111111111111111\r\n");

    GPIO_ResetBits(ADS_A0_PORT, ADS_A0_PIN);
    GPIO_ResetBits(ADS_A1_PORT, ADS_A1_PIN);
    printf("111111111111111\r\n");

    printf("[ADS] GPIO配置完成 (PDWN=PB0, SCLK=PA0, DOUT=PA1, A0=PB2, A1=PB5)\r\n");
}

//================================================
// 函数: ADS1234_Init
//================================================
//void ADS1234_Init(void)
//{
//    printf("[ADS] 开始初始化...\r\n");
//    ADS_PDWN_LOW();
//    Delay_Ms(10);
//    ADS_SCLK_LOW();
//    Delay_Ms(10);
//    ADS_PDWN_HIGH();
//    Delay_Ms(200);
//    printf("[ADS] 初始化完成\r\n");
//}


void ADS1234_Init(void)
{
    printf("[ADS] 初始化开始...\r\n");
    ADS_PDWN_LOW();
    delay_ms(10);        // ← 改这里
    ADS_SCLK_LOW();
    delay_ms(10);        // ← 改这里
    ADS_PDWN_HIGH();
    delay_ms(200);       // ← 改这里
    printf("[ADS] 初始化完成\r\n");
    printf("[ADS] PDWN(PB1)状态=%d\r\n", GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_1));
        printf("[ADS] 初始化完成\r\n");
}



//================================================
// 函数: ADS1234_SetChannel
//================================================
void ADS1234_SetChannel(ADS1234_Channel_t channel)
{
    switch(channel)
    {
        case CHANNEL_1:
            GPIO_ResetBits(ADS_A0_PORT, ADS_A0_PIN);
            GPIO_ResetBits(ADS_A1_PORT, ADS_A1_PIN);
            printf("[通道] 切换到通道1 (A0=0,A1=0)\r\n");
            break;
        case CHANNEL_2:
            GPIO_SetBits(ADS_A0_PORT, ADS_A0_PIN);
            GPIO_ResetBits(ADS_A1_PORT, ADS_A1_PIN);
            printf("[通道] 切换到通道2 (A0=1,A1=0)\r\n");
            break;
        case CHANNEL_3:
            GPIO_ResetBits(ADS_A0_PORT, ADS_A0_PIN);
            GPIO_SetBits(ADS_A1_PORT, ADS_A1_PIN);
            printf("[通道] 切换到通道3 (A0=0,A1=1)\r\n");
            break;
        case CHANNEL_4:
            GPIO_SetBits(ADS_A0_PORT, ADS_A0_PIN);
            GPIO_SetBits(ADS_A1_PORT, ADS_A1_PIN);
            printf("[通道] 切换到通道4 (A0=1,A1=1)\r\n");
            break;
    }
    delay_us(100);
}

//================================================
// 函数: ADS1234_ReadData
//================================================
//int32_t ADS1234_ReadData(ADS1234_Channel_t channel)
//{
//    int32_t adc_value = 0;
//    uint8_t i;
//    uint32_t timeout = 500000;
//    uint32_t retry_count = 0;
//
//    printf("[读数] 准备读取通道%d...\r\n", channel + 1);
//    ADS1234_SetChannel(channel);
//    delay_ms(250);   // ★★★ 只加这一行，等待ADS1234完成首次转换 ★★★
//
//
//    printf("[读数] 等待DOUT变低...\r\n");
//
//    while (ADS_DOUT_READ() == 1)
//    {
//        if (--timeout == 0)
//        {
//            retry_count++;
//            if(retry_count > 3)
//            {
//                printf("[错误] 通道%d 无响应! DOUT一直为高\r\n", channel + 1);
//                return 0;
//            }
//            printf("[重试] 通道%d 第%d次重试...\r\n", channel + 1, retry_count);
//            timeout = 50000;
//            delay_ms(10);
//        }
//    }
//
//    printf("[读数] DOUT已变低，开始读取24位数据...\r\n");
//
//    for (i = 0; i < 24; i++)
//    {
//        adc_value <<= 1;
//        ADS_SCLK_HIGH();
//        delay_us(5);
//        if (ADS_DOUT_READ())
//        {
//            adc_value |= 0x00000001;
//        }
//        ADS_SCLK_LOW();
//        delay_us(5);
//    }
//
//    printf("[读数] 发送第25个时钟脉冲...\r\n");
//    ADS_SCLK_HIGH();
//    delay_us(5);
//    ADS_SCLK_LOW();
//    delay_us(5);
//
//    if (adc_value & 0x00800000)
//    {
//        adc_value |= 0xFF000000;
//        printf("[读数] 符号扩展为负数\r\n");
//    }
//
//    printf("[读数] 通道%d 读取完成, ADC=%ld (0x%06lX)\r\n",
//           channel + 1, (long)adc_value, (uint32_t)(adc_value & 0xFFFFFF));
//
//    return adc_value;
//}


int32_t ADS1234_ReadData(ADS1234_Channel_t channel)
{
    int32_t adc_value = 0;
    uint8_t i;
    uint32_t elapsed = 0;

    ADS1234_SetChannel(channel);

    // 切换通道后，等当前转换完成 + 新通道首次转换
    // 10SPS: 100ms + 100ms = 200ms，多加余量
    delay_ms(250);

    // 超时等待 DOUT 变低
    while (ADS_DOUT_READ() == 1)
    {
        delay_ms(1);
        if (++elapsed > 500)   // 最多等 500ms
        {

            // 在上面加一行：
            printf("[调试] A0实际=%d, A1实际=%d\r\n",
                   GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_2),
                   GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_5));

            printf("[错误] 通道%d DOUT超时!\r\n", channel + 1);
            return 0;
        }
    }

    // === 读取 24 位数据（MSB 先）===
    for (i = 0; i < 24; i++)
    {
        adc_value <<= 1;
        ADS_SCLK_HIGH();
        delay_us(2);
        if (ADS_DOUT_READ())
            adc_value |= 0x00000001;
        ADS_SCLK_LOW();
        delay_us(2);
    }

    // 第 25 个时钟脉冲
    ADS_SCLK_HIGH();
    delay_us(2);
    ADS_SCLK_LOW();
    delay_us(2);

    // 符号扩展
    if (adc_value & 0x00800000)
    {
        adc_value |= 0xFF000000;
    }

    return adc_value;
}



//================================================
// 函数: ADS1234_ToVoltage
//================================================
float ADS1234_ToVoltage(int32_t adc)
{
    float voltage = (float)adc / ADS_FULL_SCALE * ADS_REF_VOLTAGE / ADS_GAIN;
    printf("[转换] ADC=%ld -> 电压=%.6f V\r\n", (long)adc, voltage);
    return voltage;
}

//================================================
// 函数: ADS1234_PrintData
//================================================
void ADS1234_PrintData(int32_t adc, float vol)
{
    static uint16_t count = 0;
    count++;

    printf(" %5d  | 0x%06lX (%8ld) | %+.6f V\r\n",
           count,
           (uint32_t)(adc & 0xFFFFFF),
           (long)adc,
           vol);
}

//================================================
// 函数: ADS1234_AutoZero
// 描述: 自动归零，3秒内读取所有通道的平均值并设置为零点
//================================================
void ADS1234_AutoZero(void)
{
    #define AUTOZERO_SAMPLES 30

    int32_t adc_sum[4] = {0};
    int32_t adc_avg[4] = {0};
    uint8_t sample_count = 0;
    uint8_t ch;

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║          自动归零校准                    ║\r\n");
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  请确保所有称台空载！                    ║\r\n");
    printf("║  正在读取ADC值，请稍候3秒...             ║\r\n");
    printf("╚══════════════════════════════════════════╝\r\n");

    for(sample_count = 0; sample_count < AUTOZERO_SAMPLES; sample_count++)
    {
        for(ch = 0; ch < 4; ch++)
        {
            int32_t adc = ADS1234_ReadData((ADS1234_Channel_t)ch);
            adc_sum[ch] += adc;
            printf(".");
        }
        delay_ms(100);
    }

    for(ch = 0; ch < 4; ch++)
    {
        adc_avg[ch] = adc_sum[ch] / AUTOZERO_SAMPLES;
        g_zero_adc[ch] = adc_avg[ch];
        g_is_calibrated[ch] = 1;
    }

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║          自动归零完成                    ║\r\n");
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  通道1 零点: %8ld                        ║\r\n", (long)g_zero_adc[0]);
    printf("║  通道2 零点: %8ld                        ║\r\n", (long)g_zero_adc[1]);
    printf("║  通道3 零点: %8ld                        ║\r\n", (long)g_zero_adc[2]);
    printf("║  通道4 零点: %8ld                        ║\r\n", (long)g_zero_adc[3]);
    printf("╚══════════════════════════════════════════╝\r\n");
}

//================================================
// 函数: ADS1234_ToWeight
// 描述: 将ADC转换为重量，并添加一阶低通滤波
//================================================
float ADS1234_ToWeight(ADS1234_Channel_t channel, int32_t adc)
{
    int32_t diff;
    float weight = 0.0f;

    static float weight_data[4] = {0};
    static int32_t adc_data[4] = {0};
    static int32_t zero_data[4] = {0};
    static int32_t diff_data[4] = {0};
    static uint8_t channel_received[4] = {0};

    // ========== 动态滤波变量 ==========
    static float prev_weight[4] = {0.0f};

    if(!g_is_calibrated[channel] || g_scale_factor[channel] == 0.0f)
    {
        printf("[重量] 通道%d 未校准! ADC=%ld\r\n", channel + 1, (long)adc);
        return 0.0f;
    }

    //diff = adc - g_zero_adc[channel];
    diff = g_zero_adc[channel]-adc;
    weight = (float)diff * g_scale_factor[channel];

    if(weight < 0) weight = 0;

    // ========== 动态滤波（变化大时立即响应，稳定时平滑）==========
    float diff_weight = weight - prev_weight[channel];
    if(diff_weight < 0) diff_weight = -diff_weight;  // 取绝对值

    float alpha;
    if(diff_weight > 10.0f)  // 变化超过10g，说明砝码正在拿放
    {
        alpha = 1.0f;  // 立即响应，不滤波
    }
    else
    {
        alpha = 0.7f;  // 稳定时轻微滤波
    }

    weight = alpha * weight + (1 - alpha) * prev_weight[channel];
    prev_weight[channel] = weight;
    // ============================================================

    weight_data[channel] = weight;
    adc_data[channel] = adc;
    zero_data[channel] = g_zero_adc[channel];
    diff_data[channel] = diff;
    channel_received[channel] = 1;

    uint8_t all_received = 1;
    for(int i = 0; i < 4; i++)
    {
        if(channel_received[i] == 0)
        {
            all_received = 0;
            break;
        }
    }

    if(all_received == 1)
    {
        int32_t weight_int[4];
        int32_t total_weight = 0;
        char screen_buf[80];

        for(int i = 0; i < 4; i++)
        {
            weight_int[i] = (int32_t)(weight_data[i] * 100);
            total_weight += weight_int[i];

            int32_t kg_int = (int32_t)(weight_data[i] * 1000);

            printf("[重量] 通道%d: ADC=%8ld, Zero=%8ld, Diff=%8ld, 重量=%3d.%02d g, 重量=%d.%02d kg\r\n",
                   i + 1,
                   (long)adc_data[i], (long)zero_data[i], (long)diff_data[i],
                   weight_int[i] / 100, weight_int[i] % 100,
                   kg_int / 1000000, (kg_int / 10000) % 100);
        }

        printf("[总重] 通道1+2+3+4 = %3d.%02d g\r\n",
               total_weight / 100, total_weight % 100);

        sprintf(screen_buf, "t5.txt=\"总重:%3d.%02d g\"\xff\xff\xff",
                total_weight / 100, total_weight % 100);
        USART1_SendString(screen_buf);

        for(int i = 0; i < 4; i++)
        {
            channel_received[i] = 0;
        }
    }

    return weight;
}
//float ADS1234_ToWeight(ADS1234_Channel_t channel, int32_t adc)
//{
//    int32_t diff;
//    float weight = 0.0f;
//
//    static float weight_data[4] = {0};
//    static int32_t adc_data[4] = {0};
//    static int32_t zero_data[4] = {0};
//    static int32_t diff_data[4] = {0};
//    static uint8_t channel_received[4] = {0};
//
//    // ========== 动态滤波变量 ==========
//    static float prev_weight[4] = {0.0f};
//
//    // ========== 串扰补偿矩阵 ==========
//    static float raw_weight[4] = {0};  // 存储原始重量
//    static const float crosstalk[4][4] = {
//        {1.00, 0.00, 0.00, 0.00},
//        {0.00, 1.00, 0.05, 0.00},  // 通道3对通道2的影响
//        {0.00, 0.05, 1.00, 0.00},  // 通道2对通道3的影响
//        {0.00, 0.00, 0.00, 1.00},
//    };
//
//    if(!g_is_calibrated[channel] || g_scale_factor[channel] == 0.0f)
//    {
//        printf("[重量] 通道%d 未校准! ADC=%ld\r\n", channel + 1, (long)adc);
//        return 0.0f;
//    }
//
//    diff = adc - g_zero_adc[channel];
//    weight = (float)diff * g_scale_factor[channel];
//    if(weight < 0) weight = 0;
//
//    // ========== 动态滤波（变化大时立即响应，稳定时平滑）==========
//    float diff_weight = weight - prev_weight[channel];
//    if(diff_weight < 0) diff_weight = -diff_weight;
//
//    float alpha;
//    if(diff_weight > 10.0f)  // 变化超过10g，快速响应
//    {
//        alpha = 1.0f;
//    }
//    else
//    {
//        alpha = 0.7f;
//    }
//
//    weight = alpha * weight + (1 - alpha) * prev_weight[channel];
//    prev_weight[channel] = weight;
//    // ============================================================
//
//    // 存储原始重量（滤波后的）
//    raw_weight[channel] = weight;
//
//    weight_data[channel] = weight;
//    adc_data[channel] = adc;
//    zero_data[channel] = g_zero_adc[channel];
//    diff_data[channel] = diff;
//    channel_received[channel] = 1;
//
//    // 检查是否所有4个通道都读取完毕
//    uint8_t all_received = 1;
//    for(int i = 0; i < 4; i++)
//    {
//        if(channel_received[i] == 0)
//        {
//            all_received = 0;
//            break;
//        }
//    }
//
//    // 当4个通道都读取完后，进行串扰补偿并打印
//    if(all_received == 1)
//    {
//        // ========== 串扰补偿 ==========
//        float compensated[4] = {0};
//        for(int i = 0; i < 4; i++)
//        {
//            compensated[i] = 0;
//            for(int j = 0; j < 4; j++)
//            {
//                compensated[i] += crosstalk[i][j] * raw_weight[j];
//            }
//            if(compensated[i] < 0) compensated[i] = 0;
//        }
//
//        // 用补偿后的重量替换
//        for(int i = 0; i < 4; i++)
//        {
//            weight_data[i] = compensated[i];
//        }
//        // =============================
//
//        int32_t weight_int[4];
//        int32_t total_weight = 0;
//        char screen_buf[80];
//
//        for(int i = 0; i < 4; i++)
//        {
//            weight_int[i] = (int32_t)(weight_data[i] * 100);
//            total_weight += weight_int[i];
//
//            int32_t kg_int = (int32_t)(weight_data[i] * 1000);
//
//            printf("[重量] 通道%d: ADC=%8ld, Zero=%8ld, Diff=%8ld, 重量=%3d.%02d g, 重量=%d.%02d kg\r\n",
//                   i + 1,
//                   (long)adc_data[i], (long)zero_data[i], (long)diff_data[i],
//                   weight_int[i] / 100, weight_int[i] % 100,
//                   kg_int / 1000000, (kg_int / 10000) % 100);
//        }
//
//        printf("[总重] 通道1+2+3+4 = %3d.%02d g\r\n",
//               total_weight / 100, total_weight % 100);
//
//        sprintf(screen_buf, "t5.txt=\"总重:%3d.%02d g\"\xff\xff\xff",
//                total_weight / 100, total_weight % 100);
//        USART1_SendString(screen_buf);
//
//        // 重置标志
//        for(int i = 0; i < 4; i++)
//        {
//            channel_received[i] = 0;
//        }
//    }
//
//    return weight;
//}
//================================================
// 函数: ADS1234_CalibrateZero
//================================================
void ADS1234_CalibrateZero(ADS1234_Channel_t channel)
{
    int32_t sum = 0;
    uint8_t i;

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║       通道%d 零点校准                    ║\r\n", channel + 1);
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  请确保该通道称台空载！                  ║\r\n");
    printf("║  正在读取ADC值，请稍候...                ║\r\n");
    printf("╚══════════════════════════════════════════╝\r\n");

    for(i = 0; i < 10; i++)
    {
        sum += ADS1234_ReadData(channel);
        delay_ms(100);
        printf(".");
    }

    g_zero_adc[channel] = sum / 10;
    g_is_calibrated[channel] = 1;

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║       通道%d 零点校准完成                ║\r\n", channel + 1);
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  零点ADC值: %8ld (0x%06lX)               ║\r\n",
           (long)g_zero_adc[channel], (uint32_t)(g_zero_adc[channel] & 0xFFFFFF));
    printf("╚══════════════════════════════════════════╝\r\n");
}

//================================================
// 函数: ADS1234_CalibrateScale
//================================================
void ADS1234_CalibrateScale(ADS1234_Channel_t channel, float weight_gram)
{
    int32_t sum = 0;
    uint8_t i;
    int32_t diff_adc;

    if(g_zero_adc[channel] == 0)
    {
        printf("[错误] 通道%d 请先进行零点校准！\r\n", channel + 1);
        return;
    }

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║       通道%d 量程校准                    ║\r\n", channel + 1);
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  请放置 %.1f 克标准砝码！                ║\r\n", weight_gram);
    printf("║  正在读取ADC值，请稍候...                ║\r\n");
    printf("╚══════════════════════════════════════════╝\r\n");

    for(i = 0; i < 10; i++)
    {
        sum += ADS1234_ReadData(channel);
        delay_ms(100);
        printf(".");
    }

    g_known_adc[channel] = sum / 10;
    g_known_weight[channel] = weight_gram;

    diff_adc = g_known_adc[channel] - g_zero_adc[channel];
    if(diff_adc != 0)
    {
        g_scale_factor[channel] = weight_gram / diff_adc;
    }
    else
    {
        g_scale_factor[channel] = 0;
        printf("\r\n[错误] ADC差值无效！\r\n");
        return;
    }

    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║       通道%d 量程校准完成                ║\r\n", channel + 1);
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  带载ADC值: %8ld                        ║\r\n", (long)g_known_adc[channel]);
    printf("║  零点ADC值: %8ld                        ║\r\n", (long)g_zero_adc[channel]);
    printf("║  ADC差值: %8ld                          ║\r\n", (long)diff_adc);
    printf("║  比例系数: %.6f g/ADC                   ║\r\n", g_scale_factor[channel]);
    printf("╚══════════════════════════════════════════╝\r\n");
}

//================================================
// 函数: ADS1234_PrintCalibration
//================================================
void ADS1234_PrintCalibration(ADS1234_Channel_t channel)
{
    printf("\r\n");
    printf("╔══════════════════════════════════════════╗\r\n");
    printf("║       通道%d 校准参数                    ║\r\n", channel + 1);
    printf("╠══════════════════════════════════════════╣\r\n");
    printf("║  零点ADC: %8ld                          ║\r\n", (long)g_zero_adc[channel]);
    printf("║  比例系数: %.6f g/ADC                   ║\r\n", g_scale_factor[channel]);
    if(g_known_weight[channel] > 0)
    {
        printf("║  校准重量: %.1f g                        ║\r\n", g_known_weight[channel]);
        printf("║  校准ADC: %8ld                          ║\r\n", (long)g_known_adc[channel]);
    }
    printf("║  校准状态: %s                              ║\r\n", g_is_calibrated[channel] ? "已校准" : "未校准");
    printf("╚══════════════════════════════════════════╝\r\n");
}

//================================================
// 函数: ADS1234_SetCalibration
//================================================
void ADS1234_SetCalibration(ADS1234_Channel_t channel, int32_t zero_adc, float scale_factor)
{
    g_zero_adc[channel] = zero_adc;
    g_scale_factor[channel] = scale_factor;
    g_is_calibrated[channel] = 1;
    printf("通道%d 校准参数已手动设置: 零点=%ld, 系数=%.6f\r\n",
           channel + 1, (long)zero_adc, scale_factor);
}
