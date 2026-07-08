#include "Card.h"
#include "ch32v30x.h"
#include "System.h"      // 用于延时函数和GetSysTick
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/******************************************************************
 * 函 数 名 称：RC522_Init
 * 函 数 说 明：IC卡感应模块配置
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：H
 * 备       注：基于CH32V307，使用PA1-PA5作为RC522接口
******************************************************************/

/*******************************
*连线说明：RC522 ---- CH32
*1--SDA  <----->PA4
*2--SCK  <----->PA5
*3--MOSI <----->PA7
*4--MISO <----->PA6
*5--悬空
*6--GND <----->GND
*7--RST <----->PB0
*8--VCC <----->3.3V
************************************/

void RC522_Init(void)
{
//    GPIO_InitTypeDef GPIO_InitStructure = {0};
//
//    // 开启GPIO时钟（根据实际使用的GPIO端口修改）
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
//
//    // 配置CS(片选)、SCK(时钟)、MOSI(主出从入)、RST(复位)为推挽输出模式
//    // SDA (CS) - PA4, SCK - PA5, MOSI - PA7, MISO - PA6
//    GPIO_InitStructure.GPIO_Pin = GPIO_CS | GPIO_SCK | GPIO_MOSI;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;       //推挽输出
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;      //10MHz速度  降低速度提高稳定性
//    GPIO_Init(PORT_GPIO, &GPIO_InitStructure);
//
//    // 初始化 CS、SCK、MOSI、RST 为高电平
//    GPIO_WriteBit(PORT_GPIO, GPIO_CS, Bit_SET);
//    GPIO_WriteBit(PORT_GPIO, GPIO_SCK, Bit_RESET);// SCK 空闲低电平
//    GPIO_WriteBit(PORT_GPIO, GPIO_MOSI, Bit_SET);
//    GPIO_WriteBit(PORT_GPIO, GPIO_RST, Bit_SET);  // RST 默认高电平
//
//    // ========== 配置 GPIOA MISO(PA6) 为上拉输入 ==========   // MISO - PA6
//    GPIO_InitStructure.GPIO_Pin = GPIO_MISO;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          //上拉输入
//    GPIO_Init(GPIOA, &GPIO_InitStructure);
//
//
//    // ========== 配置 GPIOB RST(PB0) 为推挽输出 ==========
//    GPIO_InitStructure.GPIO_Pin = GPIO_RST;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    GPIO_Init(PORT_RST, &GPIO_InitStructure);
//
//    Delay_Ms(10);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_SPI1, ENABLE);

    // 配置 SCK(PA5)、MOSI(PA7)、MISO(PA6) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置 CS(PA4) 为推挽输出（软件控制）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);  // CS 默认高电平

    // 配置 MISO(PA6) 为上拉输入
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置 RST(PB0) 为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_RST;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(PORT_RST, &GPIO_InitStructure);
    GPIO_SetBits(PORT_RST, GPIO_RST);  // RST 默认高电平

    // 配置硬件 SPI1
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;      // CPOL=0
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;    // CPHA=0
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;       // 软件控制 NSS
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; // 慢速开始
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &SPI_InitStructure);

    // 使能 SPI1
    SPI_Cmd(SPI1, ENABLE);
    RC522_Write_Register(TxModeReg, 0x8D);   // 100% ASK 调制
    RC522_Write_Register(TxControlReg, 0x83); // 开启 TX1/TX2
}

//////////////////软件模拟SPI与RC522通信///////////////////////////////////////////
///* 软件模拟SPI发送一个字节数据，高位先行 */
///**
//  * @brief  软件模拟SPI发送一个字节数据
//  * @param  byte：要发送的字节数据
//  * @retval 无
//  * @note   高位先行，时钟极性：空闲时SCK为高电平
//  */
//void RC522_SPI_SendByte(uint8_t byte)
//{
//    uint8_t n;
//    for(n = 0; n < 8; n++)          // 循环8次，发送8个bit
//    {
//        if(byte & 0x80)            // 判断当前最高位是否为1
//            RC522_MOSI_1();        // MOSI输出高电平
//        else
//            RC522_MOSI_0();        // MOSI输出低电平
//
//        // CH32V307速度较快，可以减小延时
//        delay_us(5);              // 延时，确保数据稳定
//        RC522_SCK_1();            // SCK拉低，产生时钟下降沿
//        delay_us(5);              // 延时，等待从设备采样
//        RC522_SCK_0();            // SCK拉高，完成一个时钟周期
//        delay_us(5);              // 延时
//
//        byte <<= 1;               // 左移一位，准备发送下一位
//    }
//}
//
///* 软件模拟SPI读取一个字节数据，先读高位 */
///**
//  * @brief  软件模拟SPI读取一个字节数据
//  * @param  无
//  * @retval 读取到的字节数据
//  * @note   高位先行，在SCK上升沿读取数据
//  */
//uint8_t RC522_SPI_ReadByte(void)
//{
//    uint8_t n, data = 0;
//    for(n = 0; n < 8; n++)           // 循环8次，读取8个bit
//    {
//        data <<= 1;                 // 左移一位，为接收新数据准备
//        RC522_SCK_1();             // SCK拉低，准备产生时钟
//        delay_us(5);              // 延时
//
//        if(RC522_MISO_GET())
//            data |= 0x01;         // 如果MISO为高电平，则数据位为1
//
//        RC522_SCK_0();           // SCK拉高，完成一个时钟周期
//        delay_us(5);
//    }
//    return data;                 // 返回读取到的字节
//}

// 硬件 SPI 收发一个字节
uint8_t RC522_SPI_TransferByte(uint8_t data)
{
    // 等待发送缓冲区为空
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    // 发送数据
    SPI_I2S_SendData(SPI1, data);
    // 等待接收完成
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    // 返回接收到的数据
    return SPI_I2S_ReceiveData(SPI1);
}

// 读 RC522 寄存器
uint8_t RC522_Read_Register(uint8_t Address)
{
    uint8_t data;
    uint8_t Addr = ((Address << 1) & 0x7E) | 0x80;  // 读命令

    RC522_CS_Enable();
    RC522_SPI_TransferByte(Addr);   // 发送地址
    data = RC522_SPI_TransferByte(0x00);  // 发送哑数据，读回值
    RC522_CS_Disable();

    return data;
}

// 写 RC522 寄存器
void RC522_Write_Register(uint8_t Address, uint8_t data)
{
    uint8_t Addr = (Address << 1) & 0x7E;  // 写命令

    RC522_CS_Enable();
    RC522_SPI_TransferByte(Addr);   // 发送地址
    RC522_SPI_TransferByte(data);   // 发送数据
    RC522_CS_Disable();
}

//////////////////////////CH32V307对RC522寄存器的操作//////////////////////////
/**
  * @brief  ：读取RC522指定寄存器的值
  * @param  ：Address:寄存器的地址（0x00-0x3F）
  * @retval ：寄存器的值
  * @note   ：RC522通信协议：读操作时地址最高位为1
*/
//uint8_t RC522_Read_Register(uint8_t Address)
//{
//    uint8_t data, Addr;
//
//    // RC522读操作：地址左移1位，最低位为0，最高位为1
//    Addr = ((Address << 1) & 0x7E) | 0x80;
//
//    // 调试：打印要发送的地址命令
//    //printf("Read Reg: Address=0x%02X, Send Addr=0x%02X\r\n", Address, Addr);
//
//    RC522_CS_Enable();               // 片选使能，开始通信
//    RC522_SPI_SendByte(Addr);        // 发送寄存器地址（读命令）
//    data = RC522_SPI_ReadByte();     // 读取寄存器中的值
//    RC522_CS_Disable();              // 片选失能，结束通信
//
//    return data;
//}

/**
  * @brief  ：向RC522指定寄存器中写入指定的数据
  * @param  ：Address：寄存器地址
  *          data：要写入寄存器的数据
  * @retval ：无
  * @note     RC522通信协议：写操作时地址最高位为0
*/
//void RC522_Write_Register(uint8_t Address, uint8_t data)
//{
//    uint8_t Addr;
//
//    Addr = (Address << 1) & 0x7E;
//
//    RC522_CS_Enable();                // 片选使能，开始通信
//    RC522_SPI_SendByte(Addr);         // 发送寄存器地址（写命令）
//    RC522_SPI_SendByte(data);         // 发送要写入的数据
//    RC522_CS_Disable();              // 片选失能，结束通信
//}

/**
  * @brief  ：置位RC522指定寄存器的指定位
  * @param  ：Address：寄存器地址
  *          mask：置位值
  * @retval ：无
  * @note   先读后写，不影响其他位
*/
void RC522_SetBit_Register(uint8_t Address, uint8_t mask)
{
    uint8_t temp;
    /* 获取寄存器当前值 */
    temp = RC522_Read_Register(Address);
    /* 对指定位进行置位操作后，再将值写入寄存器 */
    RC522_Write_Register(Address, temp | mask);
}

/**
  * @brief  ：清位RC522指定寄存器的指定位
  * @param  ：Address：寄存器地址
  *          mask：清位值
  * @retval ：无
  * @note   先读后写，不影响其他位
*/
void RC522_ClearBit_Register(uint8_t Address, uint8_t mask)
{
    uint8_t temp;
    /* 获取寄存器当前值 */
    temp = RC522_Read_Register(Address);
    /* 对指定位进行清位操作后，再将值写入寄存器 */
    RC522_Write_Register(Address, temp & (~mask));
}

///////////////////CH32V307对RC522的基础通信//////////////////////////
/**
  * @brief  ：开启天线
  * @param  ：无
  * @retval ：无
  * @note   通过设置TxControlReg寄存器的TX1和TX2位使能天线
*/
void RC522_Antenna_On(void)
{
    uint8_t k;
    k = RC522_Read_Register(TxControlReg);   // 读取天线控制寄存器
    /* 判断天线是否开启 */
    if(!(k & 0x03))                          // 如果TX1和TX2位都为0
        RC522_SetBit_Register(TxControlReg, 0x03);  // 置位TX1和TX2，开启天线
}

/**
  * @brief  ：关闭天线
  * @param  ：无
  * @retval ：无
*/
void RC522_Antenna_Off(void)
{
    /* 直接对相应位清零 */
    RC522_ClearBit_Register(TxControlReg, 0x03);
}

/**
  * @brief  ：复位RC522
  * @param  ：无
  * @retval ：无
  * @note   通过硬件复位引脚和软件命令相结合的方式复位
*/
//void RC522_Rese(void)
//{
//    // 硬件复位：RST(PB0) 产生一个低脉冲
//    GPIO_ResetBits(PORT_RST, GPIO_RST);  // RST 拉低
//    Delay_Ms(10);
//    GPIO_SetBits(PORT_RST, GPIO_RST);    // RST 拉高
//    Delay_Ms(50);
//
//    // 软件复位
//    RC522_Write_Register(CommandReg, 0x0F);
//    Delay_Ms(10);
//
//    // 等待复位完成
//    int timeout = 200;
//    while((RC522_Read_Register(CommandReg) & 0x10) && timeout--)
//    {
//        Delay_Ms(1);
//    }
//
//    Delay_Ms(10);
//
//    // ========== 配置寄存器 ==========
//    RC522_Write_Register(ModeReg, 0x3D);        // 定义发送和接收常用模式
//    RC522_Write_Register(TReloadRegL, 30);      // 16位定时器低位
//    RC522_Write_Register(TReloadRegH, 0);       // 16位定时器高位
//    RC522_Write_Register(TModeReg, 0x8D);       // 内部定时器的设置
//    RC522_Write_Register(TPrescalerReg, 0x3E);  // 设置定时器分频系数
//    RC522_Write_Register(TxAutoReg, 0x40);      // 调制发送信号为100%ASK
//}


void RC522_Rese(void)
{
    // 硬件复位
    GPIO_ResetBits(PORT_RST, GPIO_RST);
    Delay_Ms(5);      // 从 20ms 减少到 5ms
    GPIO_SetBits(PORT_RST, GPIO_RST);
    Delay_Ms(10);     // 从 100ms 减少到 10ms

    // 软件复位
    RC522_Write_Register(CommandReg, 0x0F);
    Delay_Ms(5);      // 从 50ms 减少到 5ms

    // 等待复位完成（减少超时时间）
    int timeout = 50;  // 从 100 减少到 50
    while((RC522_Read_Register(CommandReg) & 0x10) && timeout--)
    {
        Delay_Ms(1);   // 从 5ms 减少到 1ms
    }

    // 寄存器配置（保持原有配置，但减少多余的延时）
    RC522_Write_Register(TModeReg, 0x8D);
    RC522_Write_Register(TPrescalerReg, 0x3E);
    RC522_Write_Register(TReloadRegL, 0x1A);
    RC522_Write_Register(TReloadRegH, 0x00);
    RC522_Write_Register(TxAutoReg, 0x40);
    RC522_Write_Register(ModeReg, 0x3D);

    Delay_Ms(5);      // 从 20ms 减少到 5ms

    // 清空FIFO和中断标志
    RC522_Write_Register(FIFOLevelReg, 0x80);
    RC522_Write_Register(ComIrqReg, 0x7F);
    RC522_Write_Register(DivIrqReg, 0x7F);

    Delay_Ms(5);      // 从 10ms 减少到 5ms

    // 开启天线
    RC522_Antenna_On();
    Delay_Ms(5);      // 从 20ms 减少到 5ms
}


/**
  * @brief  ：设置RC522的工作方式
  * @param  ：Type：工作方式
  * @retval ：无
  * @note   配置RC522以支持Mifare One卡片通信
*/
void RC522_Config_Type(char Type)
{
    if(Type == 'A')                          // 配置为Type A模式
    {
        RC522_ClearBit_Register(Status2Reg, 0x08);  // 清除加密标志
        RC522_Write_Register(ModeReg, 0x3D);       // 设置通信模式
        RC522_Write_Register(RxSelReg, 0x86);       // 设置接收器选择
        RC522_Write_Register(RFCfgReg, 0x7F);       // 配置接收器增益
//        RC522_Write_Register(TReloadRegL, 30);      // 定时器重装载值
//        RC522_Write_Register(TReloadRegH, 0);

        RC522_Write_Register(TModeReg, 0x8D);
        RC522_Write_Register(TPrescalerReg, 0x3E);

        RC522_Write_Register(TReloadRegL, 0xE8);
        RC522_Write_Register(TReloadRegH, 0x03);

        RC522_Write_Register(TModeReg, 0x8D);       // 定时器模式
        RC522_Write_Register(TPrescalerReg, 0x3E);  // 定时器预分频
        delay_us(20);
        /* 开天线 ，开始工作*/
        RC522_Antenna_On();
    }
}

/////////////////////////CH32V307控制RC522与M1卡的通信///////////////////////////////////////
/* 以下函数保持不变，因为它们是RC522的通信协议部分，与MCU无关 */

/**
  * @brief  ：通过RC522和ISO14443卡通讯
  * @param  ：ucCommand：RC522命令字
  *          pInData：通过RC522发送到卡片的数据
  *          ucInLenByte：发送数据的字节长度
  *          pOutData：接收到的卡片返回数据
  *          pOutLenBit：返回数据的位长度
  * @retval ：状态值MI_OK，成功，MI_NOTAGERR（无卡），MI_ERR（错误）
  * @note   该函数封装了RC522与卡片通信的全部流程
*/
char PcdComMF522(uint8_t ucCommand, uint8_t *pInData, uint8_t ucInLenByte,
                 uint8_t *pOutData, uint32_t *pOutLenBit)
{
    char cStatus = MI_ERR;
    uint8_t ucIrqEn = 0x00;      // 中断使能
    uint8_t ucWaitFor = 0x00;    // 等待标志
    uint8_t ucLastBits;          // 最后接收的有效位数
    uint8_t ucN;                 // 临时变量
    uint32_t ul;                 // 超时计数

    switch(ucCommand)
    {
        case PCD_AUTHENT:                // Mifare认证
            ucIrqEn = 0x12;              // 允许错误中断ErrIEn 允许空闲中断IdleIEn
            ucWaitFor = 0x10;            // 认证寻卡等待时候 查询空闲中断标志位
            break;

        case PCD_TRANSCEIVE:             // 接收发送 发送接收
            ucIrqEn = 0x77;              // 允许TxIEn RxIEn IdleIEn LoAlertIEn ErrIEn TimerIEn
            ucWaitFor = 0x30;            // 寻卡等待时候 查询接收中断标志位与空闲中断标志位
            break;

        default:
            break;
    }

    // 配置中断和清除标志
    RC522_Write_Register(ComIEnReg, ucIrqEn | 0x80);  // 设置中断使能
    RC522_ClearBit_Register(ComIrqReg, 0x80);         // 清除中断标志
    RC522_Write_Register(CommandReg, PCD_IDLE);       // 进入空闲模式
    RC522_SetBit_Register(FIFOLevelReg, 0x80);        // 清空FIFO缓冲区

    // 将要发送的数据写入FIFO
    for(ul = 0; ul < ucInLenByte; ul++)
        RC522_Write_Register(FIFODataReg, pInData[ul]);

    RC522_Write_Register(CommandReg, ucCommand); // 启动命令

    // 如果是发送接收命令，需要启动发送
    if(ucCommand == PCD_TRANSCEIVE)
        RC522_SetBit_Register(BitFramingReg, 0x80);

    // 等待命令执行完成（超时保护）
    ul = 5000;  // 根据时钟频率调整，操作M1卡最大等待时间25ms

    do
    {
        ucN = RC522_Read_Register(ComIrqReg);  // 读取中断标志

//        char dbg[128];
//
//        sprintf(dbg,
//                "FIFO=%d LastBits=%d Ctrl=0x%02X\r\n",
//                ucN,
//                RC522_Read_Register(ControlReg) & 0x07,
//                RC522_Read_Register(ControlReg));
//
//        UART4_SendString(dbg);

        ul--;
    } while((ul != 0) && (!(ucN & 0x01)) && (!(ucN & ucWaitFor)));

    RC522_ClearBit_Register(BitFramingReg, 0x80); // 停止发送

    if(ul != 0)  // 未超时
    {
        // 检查错误寄存器
//        if(!(RC522_Read_Register(ErrorReg) & 0x1B))

        uint8_t err = RC522_Read_Register(ErrorReg);
//
//        char dbg[64];
//
//        sprintf(dbg,
//                "ErrorReg=0x%02X ComIrq=0x%02X\r\n",
//                err,
//                ucN);
//
//        UART4_SendString(dbg);

        if(!(err & 0x1B))
        {
            cStatus = MI_OK;  // 通信成功

            // 检查定时器中断
            if(ucN & ucIrqEn & 0x01)
                cStatus = MI_NOTAGERR;  // 无卡错误

            if(ucCommand == PCD_TRANSCEIVE)
            {
                ucN = RC522_Read_Register(FIFOLevelReg); // FIFO中数据字节数
                ucLastBits = RC522_Read_Register(ControlReg) & 0x07;// 最后字节有效位数

                // 计算接收到的总位数
                if(ucLastBits)
                    *pOutLenBit = (ucN - 1) * 8 + ucLastBits;
                else
                    *pOutLenBit = ucN * 8;

                // 读取FIFO中的数据
                if(ucN == 0)
                    ucN = 1;

                if(ucN > MAXRLEN)
                    ucN = MAXRLEN;

                for(ul = 0; ul < ucN; ul++)
                    pOutData[ul] = RC522_Read_Register(FIFODataReg);
            }
        }
        else
            cStatus = MI_ERR;  // 通信错误
    }

    // 停止定时器和命令
    RC522_SetBit_Register(ControlReg, 0x80);
    RC522_Write_Register(CommandReg, PCD_IDLE);

    return cStatus;
}

/**
  * @brief  ：寻卡
  * @param  ucReq_code，寻卡方式
  *          = 0x52：寻感应区内所有符合14443A标准的卡
  *          = 0x26：寻未进入休眠状态的卡
  *         pTagType，卡片类型代码
  * @retval ：状态值MI_OK，成功   MI_ERR（失败）
  * @note   发送寻卡命令，接收卡片返回的类型代码
**/
//char PcdRequest(uint8_t ucReq_code, uint8_t *pTagType)
//{
//    char cStatus;
//    uint8_t ucComMF522Buf[MAXRLEN];  // 通信缓冲区
//    uint32_t ulLen; // 接收数据长度
////    int retry = 0;
//
//    // 清除加密标志，配置通信参数
//    RC522_ClearBit_Register(Status2Reg, 0x08);
//    RC522_Write_Register(BitFramingReg, 0x07);
//    RC522_SetBit_Register(TxControlReg, 0x03);  // 开启天线
//
//    // 清空 FIFO
//    RC522_SetBit_Register(FIFOLevelReg, 0x80);
//
//    // 发送寻卡命令
//    ucComMF522Buf[0] = ucReq_code;
//    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &ulLen);
//
//    // 解析返回的卡片类型
//    if((cStatus == MI_OK) && (ulLen == 0x10))
//    {
//        *pTagType = ucComMF522Buf[0];        // 卡片类型高字节
//        *(pTagType + 1) = ucComMF522Buf[1];  // 卡片类型低字节
//    }
//    else
//        cStatus = MI_ERR;
//
//    return cStatus;
//}


//char PcdRequest(uint8_t ucReq_code, uint8_t *pTagType)
//{
//    char cStatus;
//    uint8_t ucComMF522Buf[MAXRLEN];
//    uint32_t ulLen;
//
//    RC522_ClearBit_Register(Status2Reg, 0x08);
//    RC522_Write_Register(BitFramingReg, 0x07);   // 发送 7 位寻卡命令
//    RC522_SetBit_Register(TxControlReg, 0x03);   // 开启天线
//
//    ucComMF522Buf[0] = ucReq_code;
//    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &ulLen);
//
//    if((cStatus == MI_OK) && (ulLen == 0x10))
//    {
//        *pTagType = ucComMF522Buf[0];
//        *(pTagType + 1) = ucComMF522Buf[1];
//        return MI_OK;
//    }
//    else
//        return MI_ERR;
//}






char PcdRequest(uint8_t ucReq_code, uint8_t *pTagType)
{
    char cStatus;
    uint8_t ucComMF522Buf[MAXRLEN];
    uint32_t ulLen;

    RC522_ClearBit_Register(Status2Reg, 0x08);
    RC522_Write_Register(BitFramingReg, 0x07);
    RC522_SetBit_Register(TxControlReg, 0x03);

    ucComMF522Buf[0] = ucReq_code;

    cStatus = PcdComMF522(
                    PCD_TRANSCEIVE,
                    ucComMF522Buf,
                    1,
                    ucComMF522Buf,
                    &ulLen);

//    char dbg[128];
//
//    sprintf(dbg,
//            "Request status=0x%02X len=%lu data=%02X %02X\r\n",
//            (unsigned char)cStatus,
//            ulLen,
//            ucComMF522Buf[0],
//            ucComMF522Buf[1]);
//
//    UART4_SendString(dbg);

    if((cStatus == MI_OK) && (ulLen == 0x10))
    {
        *pTagType = ucComMF522Buf[0];
        *(pTagType + 1) = ucComMF522Buf[1];
        return MI_OK;
    }

    return MI_ERR;
}




/**
  * @brief  ：防冲突操作，获取卡片序列号
  * @param  ：pSnr：卡片序列，4字节，会返回选中卡片的序列
  * @retval ：状态值MI_OK，成功    MI_ERR（失败）
  * @note   当多张卡同时在场时，通过防冲突算法选中一张卡
*/
char PcdAnticoll(uint8_t *pSnr)
{
    char status;
    uint8_t i, ucSnr_check = 0;
    uint8_t buf[MAXRLEN];
    uint32_t ulLen;

    // 清 FIFO 和中断
    RC522_SetBit_Register(FIFOLevelReg, 0x80);
    RC522_Write_Register(CommandReg, PCD_IDLE);
    delay_us(10);

    // 配置寄存器
    RC522_ClearBit_Register(Status2Reg, 0x08);
    RC522_Write_Register(BitFramingReg, 0x00);   // 8位数据
    RC522_ClearBit_Register(CollReg, 0x80);      // 清冲突检测

    // 发送防冲突命令 (0x93, 0x20)
    buf[0] = 0x93;
    buf[1] = 0x20;
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 2, buf, &ulLen);

    if (status == MI_OK && ulLen == 0x28)  // 5字节 = 40位 = 0x28
    {
        for (i = 0; i < 4; i++) {
            pSnr[i] = buf[i];
            ucSnr_check ^= buf[i];
        }
        if (ucSnr_check != buf[4])
            status = MI_ERR;
//        else
//            UART4_SendString("Anticoll OK\r\n");
    }
    else
    {
        status = MI_ERR;
//        UART4_SendString("Anticoll failed\r\n");
    }

    RC522_SetBit_Register(CollReg, 0x80);
    return status;
}
//char PcdAnticoll(uint8_t *pSnr)
//{
//    char cStatus;
//    uint8_t uc, ucSnr_check = 0;
//    uint8_t ucComMF522Buf[MAXRLEN];
//    uint32_t ulLen;
//
//    // 初始化相关寄存器
////    RC522_ClearBit_Register(Status2Reg, 0x08);
////    RC522_Write_Register(BitFramingReg, 0x00);
////    RC522_ClearBit_Register(CollReg, 0x80);  // 清除冲突检测标志
//
//    RC522_Write_Register(Status2Reg, 0x00);
//    RC522_Write_Register(BitFramingReg, 0x00);
//    RC522_Write_Register(CollReg, 0x80);
//
//    // 发送防冲突命令
//    ucComMF522Buf[0] = 0x93;   // 防冲突命令码
//    ucComMF522Buf[1] = 0x20;   // 防冲突参数
//
//    UART4_SendString("SEND ANTICOLL 93 20\r\n");
//
////    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, &ulLen);
//
//
//    cStatus = PcdComMF522(
//                PCD_TRANSCEIVE,
//                ucComMF522Buf,
//                2,
//                ucComMF522Buf,
//                &ulLen);
//
//    char dbg[128];
//
//    sprintf(dbg,
//            "ANTI status=0x%02X len=%lu data=%02X %02X %02X %02X %02X\r\n",
//            (unsigned char)cStatus,
//            ulLen,
//            ucComMF522Buf[0],
//            ucComMF522Buf[1],
//            ucComMF522Buf[2],
//            ucComMF522Buf[3],
//            ucComMF522Buf[4]);
//
//    UART4_SendString(dbg);
//
//
////    if(cStatus == MI_OK)
////    {
////        // 提取卡片序列号
////        for(uc = 0; uc < 4; uc++)
////        {
////            *(pSnr + uc) = ucComMF522Buf[uc];   // 保存序列号
////            ucSnr_check ^= ucComMF522Buf[uc];   // 计算校验和
////        }
////
////        if(ucSnr_check != ucComMF522Buf[uc])
////            cStatus = MI_ERR;  // 校验错误
////    }
//
//
//    if((cStatus == MI_OK) && (ulLen == 40))
//    {
//        for(uc = 0; uc < 4; uc++)
//        {
//            *(pSnr + uc) = ucComMF522Buf[uc];
//            ucSnr_check ^= ucComMF522Buf[uc];
//        }
//
//        if(ucSnr_check != ucComMF522Buf[4])
//        {
//            UART4_SendString("BCC ERROR\r\n");
//            cStatus = MI_ERR;
//        }
//    }
//    else
//    {
//        UART4_SendString("UID LEN ERROR\r\n");
//        cStatus = MI_ERR;
//    }
//
//    RC522_SetBit_Register(CollReg, 0x80);  // 恢复冲突检测
//    return cStatus;
//}

/**
 * @brief   :用RC522计算CRC16
 * @param   ：pIndata：计算CRC16的数组
 *          ucLen：计算CRC16的数组字节长度
 *          pOutData：存放计算结果存放的首地址
 * @note   CRC16结果：pOutData[0]为低字节，pOutData[1]为高字节
*/
void CalulateCRC(uint8_t *pIndata, uint8_t ucLen, uint8_t *pOutData)
{
    uint8_t uc, ucN;

    // 配置CRC计算
    RC522_ClearBit_Register(DivIrqReg, 0x04);     // 清除CRC中断标志
    RC522_Write_Register(CommandReg, PCD_IDLE);   // 空闲模式
    RC522_SetBit_Register(FIFOLevelReg, 0x80);    // 清空FIFO

    // 将数据写入FIFO
    for(uc = 0; uc < ucLen; uc++)
        RC522_Write_Register(FIFODataReg, *(pIndata + uc));

    RC522_Write_Register(CommandReg, PCD_CALCCRC);// 启动CRC计算

    // 等待CRC计算完成
    uc = 0xFF;
    do
    {
        ucN = RC522_Read_Register(DivIrqReg);
        uc--;
    } while((uc != 0) && !(ucN & 0x04));

    pOutData[0] = RC522_Read_Register(CRCResultRegL);  // CRC低字节
    pOutData[1] = RC522_Read_Register(CRCResultRegM);  // CRC高字节
}

/**
  * @brief   :选定卡片
  * @param   ：pSnr：卡片序列号，4字节
  * @retval  ：状态值MI_OK，成功
  * @note    ： 在防冲突后，通过序列号选定要操作的卡片
*/
char PcdSelect(uint8_t *pSnr)
{
    char status;
    uint8_t i;
    uint8_t buf[9];
    uint32_t ulLen;

    // 清空 FIFO
    RC522_SetBit_Register(FIFOLevelReg, 0x80);
    delay_us(20);

    // 组装选卡命令
    buf[0] = 0x93;      // 防冲突命令
    buf[1] = 0x70;      // 选卡参数

    // 复制 UID（4字节）
    for(i = 0; i < 4; i++) {
        buf[i + 2] = pSnr[i];
    }

    // 计算 UID 异或校验
    buf[6] = 0;
    for(i = 0; i < 4; i++) {
        buf[6] ^= pSnr[i];
    }

//    printf("PcdSelect: buf[0-6]: ");
//    for(i = 0; i < 7; i++) printf("%02X ", buf[i]);
//    printf("\r\n");

    // 计算 CRC（7字节数据）
    CalulateCRC(buf, 7, &buf[7]);

//    printf("PcdSelect: with CRC: ");
//    for(i = 0; i < 9; i++) printf("%02X ", buf[i]);
//    printf("\r\n");

    // 发送选卡命令（9字节）
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 9, buf, &ulLen);

//    printf("PcdSelect: status=%d, ulLen=%d\r\n", status, ulLen);

//    if(status == MI_OK && ulLen > 0) {
//        printf("PcdSelect response: ");
//        for(i = 0; i < ulLen/8; i++) printf("%02X ", buf[i]);
//        printf("\r\n");
//    }


    // 检查响应：选卡成功应返回 SAK（通常是 0x08 或 0x18 或 0x20）
    if(status == MI_OK && ulLen == 0x18) {  // 24位 = 3字节
        // SAK 在 buf[0]
//        printf("SAK = 0x%02X\r\n", buf[0]);
        if(buf[0] == 0x08 || buf[0] == 0x18 || buf[0] == 0x20) {
            status = MI_OK;
        } else {
            status = MI_ERR;
        }
    } else if(status == MI_OK && ulLen == 0x90) {
        // 标准响应 144位 = 18字节
        status = MI_OK;
    } else {
        status = MI_ERR;
    }

    return status;
}
//char PcdSelect(uint8_t *pSnr)
//{
//    char ucN;
//    uint8_t uc;
//    uint8_t ucComMF522Buf[MAXRLEN];
//    uint32_t ulLen;
//
//    // 清空 FIFO（重要！）
//    RC522_SetBit_Register(FIFOLevelReg, 0x80);
//    delay_us(20);
//
////    ucComMF522Buf[0] = PICC_ANTICOLL1;  // 防冲突命令
//    // 组装选卡命令（标准格式）
//    ucComMF522Buf[0] = 0x93;            // 防冲突命令（不是0x93吗？你用的是PICC_ANTICOLL1）
//    ucComMF522Buf[1] = 0x70;            // 选定卡片参数
//    ucComMF522Buf[6] = 0;               // 校验和初始值
//
//    // 复制UID（4字节）
//    for(uc = 0; uc < 4; uc++)
//        ucComMF522Buf[uc + 2] = pSnr[uc];
//
//    // 计算UID异或校验
//    for(uc = 0; uc < 4; uc++)
//    {
//        ucComMF522Buf[uc + 2] = *(pSnr + uc);
//        ucComMF522Buf[6] ^= *(pSnr + uc);
//    }
//
//    CalulateCRC(ucComMF522Buf, 7, &ucComMF522Buf[7]);// 计算CRC校验
//
//    // 发送命令并接收响应
//    RC522_ClearBit_Register(Status2Reg, 0x08);
//    ucN = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, &ulLen);
//
//    // 检查响应
//    if((ucN == MI_OK) && (ulLen == 0x90))
//        ucN = MI_OK;
//    else
//        ucN = MI_ERR;
//
//    return ucN;
//}

/**
  * @brief   :校验卡片密码
  * @param   ：ucAuth_mode：密码验证模式
  *          = 0x60，验证A密钥
  *          = 0x61，验证B密钥
  *          ucAddr：块地址
  *          pKey：密码
  *          pSnr：卡片序列号，4字节
  * @retval  ：状态值MI_OK，成功
  * @note   验证成功后，才能对卡片进行读写操作
*/
char PcdAuthState(uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t *pKey, uint8_t *pSnr)
{
    char cStatus;
    uint8_t uc, ucComMF522Buf[MAXRLEN];
    uint32_t ulLen;

    // 组装认证命令数据包
    ucComMF522Buf[0] = ucAuth_mode;  // 认证模式
    ucComMF522Buf[1] = ucAddr;       // 块地址

    // 添加6字节密码
    for(uc = 0; uc < 6; uc++)
        ucComMF522Buf[uc + 2] = *(pKey + uc);

    // 添加4字节卡片序列号
    for(uc = 0; uc < 6; uc++)
        ucComMF522Buf[uc + 8] = *(pSnr + uc);

    cStatus = PcdComMF522(PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, &ulLen);// 发送认证命令

    // 检查认证是否成功
    if((cStatus != MI_OK) || (!(RC522_Read_Register(Status2Reg) & 0x08)))
        cStatus = MI_ERR;

    return cStatus;
}

/**
  * @brief   :在M1卡的指定块地址写入指定数据
  * @param   ：ucAddr：块地址
  *          pData：写入的数据，16字节
  * @retval  ：状态值MI_OK，成功
  * @note   写入前需要先进行密码验证
*/
char PcdWrite(uint8_t ucAddr, uint8_t *pData)
{
    char cStatus;
    uint8_t uc, ucComMF522Buf[MAXRLEN];
    uint32_t ulLen;

    ucComMF522Buf[0] = PICC_WRITE;   // 写命令
    ucComMF522Buf[1] = ucAddr;       // 块地址
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);  // 计算CRC

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);

    // 检查卡片是否准备好接收数据
    if((cStatus != MI_OK) || (ulLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
        cStatus = MI_ERR;

    if(cStatus == MI_OK)
    {
        for(uc = 0; uc < 16; uc++)
            ucComMF522Buf[uc] = *(pData + uc);

        CalulateCRC(ucComMF522Buf, 16, &ucComMF522Buf[16]); // 计算数据CRC

        cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, &ulLen);// 发送数据到卡片

        // 检查写入是否成功
        if((cStatus != MI_OK) || (ulLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
            cStatus = MI_ERR;
    }

    return cStatus;
}

/**
  * @brief   :读取M1卡的指定块地址的数据
  * @param   ：ucAddr：块地址
  *          pData：读出的数据，16字节
  * @retval  ：状态值MI_OK，成功
  * @note   读取前需要先进行密码验证
*/
char PcdRead(uint8_t ucAddr, uint8_t *pData)
{
    char cStatus;
    uint8_t uc, ucComMF522Buf[MAXRLEN];
    uint32_t ulLen;

    ucComMF522Buf[0] = PICC_READ;    // 读命令
    ucComMF522Buf[1] = ucAddr;       // 块地址
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);  // 计算CRC

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);// 发送命令并接收数据

    // 提取16字节数据
    if((cStatus == MI_OK) && (ulLen == 0x90))
    {
        for(uc = 0; uc < 16; uc++)
            *(pData + uc) = ucComMF522Buf[uc];
    }
    else
        cStatus = MI_ERR;

    return cStatus;
}

/**
  * @brief   :让卡片进入休眠模式
  * @param   ：无
  * @retval  ：状态值MI_OK，成功
  * @note   休眠后卡片不再响应寻卡命令，直到重新上电
*/
char PcdHalt(void)
{
    uint8_t ucComMF522Buf[MAXRLEN];
    uint32_t ulLen;

    // 发送休眠命令
    ucComMF522Buf[0] = PICC_HALT;    // 休眠命令
    ucComMF522Buf[1] = 0;            // 参数

    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);  // 计算CRC
    PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);// 发送命令，不等待响应

    return MI_OK;
}

// 加密密钥数组（可以自定义）
static const uint8_t ENC_KEY[16] = {
    0xA5, 0x5A, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
    0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB
};

/**
  * @brief 加密余额数据
  * @param plain 原始数据指针
  * @param cipher 加密后数据指针
  * @param len 数据长度
  */
void EncryptBalance(uint8_t *plain, uint8_t *cipher, uint8_t len)
{
    for(uint8_t i = 0; i < len; i++) {
        cipher[i] = plain[i] ^ ENC_KEY[i % 16];  // 异或加密
        // 可选：增加移位混淆
        cipher[i] = ((cipher[i] << 1) | (cipher[i] >> 7));
    }
}

/**
  * @brief 解密余额数据
  * @param cipher 加密数据指针
  * @param plain 解密后数据指针
  * @param len 数据长度
  */
void DecryptBalance(uint8_t *cipher, uint8_t *plain, uint8_t len)
{
    for(uint8_t i = 0; i < len; i++) {
        uint8_t temp = cipher[i];
        // 还原移位
        temp = (temp >> 1) | (temp << 7);
        plain[i] = temp ^ ENC_KEY[i % 16];
    }
}

/**
  * @brief 计算CRC16
  * @param data 数据指针
  * @param len 数据长度
  * @return CRC16值
  */
uint16_t CalculateCRC16(uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/**
  * @brief 计算校验和
  * @param data 数据指针
  * @param len 数据长度
  * @return 校验和
  */
uint8_t CalculateChecksum(uint8_t *data, uint8_t len)
{
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// ========== RC522调试输出函数 ==========
#include <stdio.h>
#include <stdarg.h>

void RC522_Debug_Send(const char* fmt, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    UART4_SendString(buffer);
}
