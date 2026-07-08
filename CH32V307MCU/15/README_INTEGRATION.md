# 工程4：K230识别 + 称重 + RC522 RFID 整合系统

## 功能概述
本工程整合了三个功能模块：
1. **K230物品识别** - 通过UART3接收物品识别结果
2. **四通道称重** - ADS1234称重传感器
3. **RC522 RFID** - 通过UART4输出卡片信息到串口助手

## 硬件连接
```
ADS1234称重模块:
  PA0(SCLK), PA1(DOUT), PB0(PDWN), PB2(A0), PB5(A1)

UART1: PA9(TX), PA10(RX) - 调试+串口屏
UART3: PB10(TX), PB11(RX) - 接收K230数据  
UART4: PC10(TX), PC11(RX) - 发送到串口助手

RC522 RFID:
  SDA/CS  - PA4
  SCK     - PA5
  MISO    - PA6
  MOSI    - PA7
  RST     - PB0
```

## 数据流程
1. **K230识别** → UART3 → JSON解析 → 物品信息存储
2. **称重读取** → ADS1234 → 重量数据存储
3. **数据匹配** → 物品+重量合并 → UART4输出JSON格式
4. **RFID刷卡** → RC522检测 → UART4输出卡片信息

## 串口输出格式

### UART4输出到串口助手：
1. **称重+物品合并数据**（JSON格式）：
```json
{"status":"merged","items":[{"position":"top_left","item":"apple","weight":123.45}]}
```

2. **RFID卡片信息**：
```
====
Card detected, UID: 94-2D-6A-06
Card selection success
========== User Details ==========
User ID: 888888
User Name: TestUser
User Level: Normal User
==================================
```

## 主要特性
- ✅ K230物品识别实时接收
- ✅ 四通道称重数据每2秒更新
- ✅ RC522 RFID每200ms检测一次
- ✅ 所有输出统一通过UART4发送到串口助手
- ✅ 支持管理员卡充值功能
- ✅ 支持新卡自动初始化

## 编译说明
1. 使用MounRiver IDE打开工程：`D:\BaiduNetdiskDownload\Project_MounRiver\Project\4\1.wvproj`
2. 选择目标设备：CH32V307
3. 编译并下载到开发板
4. 打开串口助手（115200波特率）连接UART4

## 调试输出
- UART1：调试信息和串口屏控制
- UART4：K230数据、称重数据、RFID卡片信息（发送到串口助手）

## 注意事项
- RC522使用SPI1接口，确保SPI配置正确
- 如果RC522检测失败，检查天线连接和供电
- UART4已配置为115200波特率
- SysTick已配置为1ms中断
