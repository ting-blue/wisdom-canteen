#ifndef BSP_RC522_H
#define BSP_RC522_H

#include "ch32v30x.h"

#ifndef u8
#define u8 uint8_t
#endif

#ifndef u16
#define u16 uint16_t
#endif

#ifndef u32
#define u32 uint32_t
#endif

// ========== �˿ڶ��� ==========
// ========== ʱ��ʹ�� ==========
#define RCC_GPIO_ENABLE()   do { \
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); \
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); \
} while(0)
//#define RCC_GPIO_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
#define PORT_GPIO           GPIOA   // CS, SCK, MOSI, MISO
#define PORT_RST            GPIOB   // RST ������ GPIOB

// ========== ���Ŷ��� ==========
// SDA (CS) - PA4
#define GPIO_CS             GPIO_Pin_4  // PA4 - Ƭѡ
// SCK - PA5
#define GPIO_SCK            GPIO_Pin_5  // PA5 - ʱ��
// MOSI - PA7
#define GPIO_MOSI           GPIO_Pin_7  // PA7 - MOSI
// RST - PB0
#define GPIO_RST            GPIO_Pin_0  // PB0 - ��λ
// MISO - PA6
#define GPIO_MISO           GPIO_Pin_6  // PA6 - MISO

/* IO�ڲ������� */
//#define RC522_CS_Enable()         GPIO_WriteBit(PORT_GPIO, GPIO_CS, Bit_RESET)// CS�͵�ƽʹ��
//#define RC522_CS_Disable()        GPIO_WriteBit(PORT_GPIO, GPIO_CS, Bit_SET)// CS�ߵ�ƽ����

#define RC522_CS_Enable()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define RC522_CS_Disable()  GPIO_SetBits(GPIOA, GPIO_Pin_4)

// RST ���ƣ�PB0��
#define RC522_Reset_Enable()   GPIO_ResetBits(PORT_RST, GPIO_RST)   // ���͸�λ
#define RC522_Reset_Disable()  GPIO_SetBits(PORT_RST, GPIO_RST)     // �ͷŸ�λ

#define RC522_SCK_0()             GPIO_WriteBit(PORT_GPIO, GPIO_SCK, Bit_RESET)
#define RC522_SCK_1()             GPIO_WriteBit(PORT_GPIO, GPIO_SCK, Bit_SET)

#define RC522_MOSI_0()            GPIO_WriteBit(PORT_GPIO, GPIO_MOSI, Bit_RESET)
#define RC522_MOSI_1()            GPIO_WriteBit(PORT_GPIO, GPIO_MOSI, Bit_SET)

#define RC522_MISO_GET()          GPIO_ReadInputDataBit(PORT_GPIO, GPIO_MISO)

//RC522������
#define PCD_IDLE              0x00               //ȡ����ǰ����
#define PCD_AUTHENT           0x0E               //��֤��Կ
#define PCD_RECEIVE           0x08               //��������
#define PCD_TRANSMIT          0x04               //��������
#define PCD_TRANSCEIVE        0x0C               //���Ͳ���������
#define PCD_RESETPHASE        0x0F               //��λ
#define PCD_CALCCRC           0x03               //CRC����

//Mifare_One��Ƭ������
#define PICC_REQIDL           0x26               //Ѱ��������δ��������״̬
#define PICC_REQALL           0x52               //Ѱ��������ȫ����
#define PICC_ANTICOLL1        0x93               //����ײ
#define PICC_ANTICOLL2        0x95               //����ײ
#define PICC_AUTHENT1A        0x60               //��֤A��Կ
#define PICC_AUTHENT1B        0x61               //��֤B��Կ
#define PICC_READ             0x30               //����
#define PICC_WRITE            0xA0               //д��
#define PICC_DECREMENT        0xC0               //�ۿ�
#define PICC_INCREMENT        0xC1               //��ֵ
#define PICC_RESTORE          0xC2               //�������ݵ�������
#define PICC_TRANSFER         0xB0               //���滺����������
#define PICC_HALT             0x50               //����

/* RC522  FIFO���ȶ��� */
#define DEF_FIFO_LENGTH       64                 //FIFO size=64byte
#define MAXRLEN               18

/* RC522�Ĵ������� */
// PAGE 0
#define     RFU00                 0x00    //����
#define     CommandReg            0x01    //������ֹͣ�����ִ��
#define     ComIEnReg             0x02    //�ж����󴫵ݵ�ʹ�ܣ�Enable/Disable��
#define     DivlEnReg             0x03    //�ж����󴫵ݵ�ʹ��
#define     ComIrqReg             0x04    //�����ж������־
#define     DivIrqReg             0x05    //�����ж������־
#define     ErrorReg              0x06    //�����־��ָʾִ�е��ϸ�����Ĵ���״̬
#define     Status1Reg            0x07    //����ͨ�ŵ�״̬��ʶ
#define     Status2Reg            0x08    //�����������ͷ�������״̬��־
#define     FIFODataReg           0x09    //64�ֽ�FIFO����������������
#define     FIFOLevelReg          0x0A    //ָʾFIFO�д洢���ֽ���
#define     WaterLevelReg         0x0B    //����FIFO��������籨����FIFO���
#define     ControlReg            0x0C    //��ͬ�Ŀ��ƼĴ���
#define     BitFramingReg         0x0D    //����λ��֡�ĵ���
#define     CollReg               0x0E    //RF�ӿ��ϼ�⵽�ĵ�һ��λ��ͻ��λ��λ��
#define     RFU0F                 0x0F    //����
// PAGE 1
#define     RFU10                 0x10    //����
#define     ModeReg               0x11    //���巢�ͺͽ��յĳ���ģʽ
#define     TxModeReg             0x12    //���巢�͹��̵����ݴ�������
#define     RxModeReg             0x13    //������չ����е����ݴ�������
#define     TxControlReg          0x14    //���������������ܽ�TX1��TX2���߼�����
#define     TxAutoReg             0x15    //��������������������
#define     TxSelReg              0x16    //ѡ���������������ڲ�Դ
#define     RxSelReg              0x17    //ѡ���ڲ��Ľ���������
#define     RxThresholdReg        0x18    //ѡ��λ����������ֵ
#define     DemodReg              0x19    //��������������
#define     RFU1A                 0x1A    //����
#define     RFU1B                 0x1B    //����
#define     MifareReg             0x1C    //����ISO 14443/MIFAREģʽ��106kbit/s��ͨ��
#define     RFU1D                 0x1D    //����
#define     RFU1E                 0x1E    //����
#define     SerialSpeedReg        0x1F    //ѡ����UART�ӿڵ�����
// PAGE 2
#define     RFU20                 0x20    //����
#define     CRCResultRegM         0x21    //��ʾCRC�����ʵ��MSBֵ
#define     CRCResultRegL         0x22    //��ʾCRC�����ʵ��LSBֵ
#define     RFU23                 0x23    //����
#define     ModWidthReg           0x24    //����ModWidth������
#define     RFU25                 0x25    //����
#define     RFCfgReg              0x26    //���ý���������
#define     GsNReg                0x27    //ѡ�������������ܽţ�TX1��TX2���ĵ��Ƶ絼
#define     CWGsCfgReg            0x28    //ѡ�������������ܽŵĵ��Ƶ絼
#define     ModGsCfgReg           0x29    //ѡ�������������ܽŵĵ��Ƶ絼
#define     TModeReg              0x2A    //�����ڲ���ʱ��������
#define     TPrescalerReg         0x2B    //�����ڲ���ʱ��������
#define     TReloadRegH           0x2C    //����16λ���Ķ�ʱ����װֵ
#define     TReloadRegL           0x2D    //����16λ���Ķ�ʱ����װֵ
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F    //��ʾ16λ����ʵ�ʶ�ʱ��ֵ
// PAGE 3
#define     RFU30                 0x30    //����
#define     TestSel1Reg           0x31    //���ò����ź�����
#define     TestSel2Reg           0x32    //���ò����ź����ú�PRBS����
#define     TestPinEnReg          0x33    //D1-D7�����������ʹ�ܹܽţ������ڴ��нӿڣ�
#define     TestPinValueReg       0x34    //����D1-D7����I/O����ʱ��ֵ
#define     TestBusReg            0x35    //��ʾ�ڲ��������ߵ�״̬
#define     AutoTestReg           0x36    //���������Բ���
#define     VersionReg            0x37    //��ʾ�汾
#define     AnalogTestReg         0x38    //���ƹܽ�AUX1��AUX2
#define     TestDAC1Reg           0x39    //����TestDAC1�Ĳ���ֵ
#define     TestDAC2Reg           0x3A    //����TestDAC2�Ĳ���ֵ
#define     TestADCReg            0x3B    //��ʾADCI��Qͨ����ʵ��ֵ
#define     RFU3C                 0x3C    //����
#define     RFU3D                 0x3D    //����
#define     RFU3E                 0x3E    //����
#define     RFU3F                                          0x3F    //����

/* ��RC522ͨ��ʱ���صĴ������ */
#define         MI_OK                 0x26
#define         MI_NOTAGERR           0xcc
#define         MI_ERR                0xbb

/**********************************************************************/

void RC522_Init(void);/* IO�ڳ�ʼ�� */

////////////////����ģ��SPI��RC522ͨ��///////////////////////////////////////////
void RC522_SPI_SendByte( uint8_t byte );/* ����ģ��SPI����һ���ֽ����ݣ���λ���� */
uint8_t RC522_SPI_ReadByte( void );/* ����ģ��SPI��ȡһ���ֽ����ݣ��ȶ���λ */
uint8_t RC522_Read_Register( uint8_t Address );//��ȡRC522ָ���Ĵ�����ֵ
void RC522_Write_Register( uint8_t Address, uint8_t data );//��RC522ָ���Ĵ�����д��ָ��������
void RC522_SetBit_Register( uint8_t Address, uint8_t mask );//��λRC522ָ���Ĵ�����ָ��λ
void RC522_ClearBit_Register( uint8_t Address, uint8_t mask );//��λRC522ָ���Ĵ�����ָ��λ

/////////////////////STM32��RC522�Ļ���ͨ��///////////////////////////////////
void RC522_Antenna_On( void );//��������
void RC522_Antenna_Off( void );//�ر�����
void RC522_Rese( void );//��λRC522
void RC522_Config_Type( char Type );//����RC522�Ĺ�����ʽ

/////////////////////////STM32����RC522��M1��ͨ��///////////////////////////////////////
char PcdComMF522 ( uint8_t ucCommand, uint8_t * pInData, uint8_t ucInLenByte, uint8_t * pOutData, uint32_t * pOutLenBit );//ͨ��RC522��ISO14443��ͨѶ
char PcdRequest ( uint8_t ucReq_code, uint8_t * pTagType );//Ѱ��
char PcdAnticoll ( uint8_t * pSnr );//����ͻ
void CalulateCRC ( uint8_t * pIndata, u8 ucLen, uint8_t * pOutData );//��RC522����CRC16��ѭ������У�飩
char PcdSelect ( uint8_t * pSnr );//ѡ����Ƭ
char PcdAuthState ( uint8_t ucAuth_mode, uint8_t ucAddr, uint8_t * pKey, uint8_t * pSnr );//У�鿨Ƭ����
char PcdWrite ( uint8_t ucAddr, uint8_t * pData );//��M1����ָ�����ַд��ָ������
char PcdRead ( uint8_t ucAddr, uint8_t * pData );//��ȡM1����ָ�����ַ������
char PcdHalt( void );//�ÿ�Ƭ��������ģʽ
void EncryptBalance(uint8_t *plain, uint8_t *cipher, uint8_t len);//�����������
void DecryptBalance(uint8_t *cipher, uint8_t *plain, uint8_t len);//�����������
uint16_t CalculateCRC16(uint8_t *data, uint8_t len);//����CRC16
uint8_t CalculateChecksum(uint8_t *data, uint8_t len);//����У���


#define USER_INFO_BLOCK    20   // ����5��0���洢�û���Ϣ
#define USER_INFO_SECTOR   5    // ʹ������5

typedef struct {
    uint32_t user_id;           // �û�ID��4�ֽڣ�
    uint8_t user_name[12];      // �û�����12�ֽڣ�
    uint8_t user_level;         // �û��ȼ���1�ֽڣ���1-��ͨ��2-����Ա
    uint16_t department;        // ���Ŵ��루2�ֽڣ�
    uint8_t reserved[5];        // Ԥ����5�ֽڣ�
} UserData_t;  // �ܹ�24�ֽڣ���Ҫ2����洢


extern uint8_t card_is_activated;
extern uint8_t user_state;

// ========== UART4支持 ==========
extern void UART4_SendString(char *str);
extern void UART4_SendByte(uint8_t data);

// ========== RC522调试输出 ==========
#define RC522_DEBUG_ENABLE 1  // 1=开启调试输出到UART4, 0=关闭

#if RC522_DEBUG_ENABLE
    void RC522_Debug_Send(const char* fmt, ...);
    #define RC522_DBG(...) RC522_Debug_Send(__VA_ARGS__)
#else
    #define RC522_DBG(...)
#endif

#endif
