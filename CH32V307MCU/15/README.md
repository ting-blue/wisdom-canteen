# 项目"1" - 称重+K230位置匹配系统

## 项目信息

- **项目名称**: 1
- **功能**: 四通道称重 + K230物品位置识别匹配
- **芯片**: CH32V307 (RISC-V)
- **IDE**: MounRiver

## 项目结构

```
1/
├── .cproject              # 构建配置
├── .project               # 项目描述（名称已改为"1"）
├── .settings/             # IDE设置
├── 1.launch               # 调试配置
├── 1.wvproj              # MounRiver项目文件
├── Core/                  # RISC-V核心文件
├── Debug/                 # 调试文件
├── Ld/                    # 链接脚本
├── Peripheral/            # 外设驱动库
├── Startup/               # 启动文件
└── User/                  # 用户代码
    ├── main.c            # 主程序（位置匹配功能）
    ├── ADS1234.c/.h      # ADS1234驱动
    ├── System.c/.h       # 系统配置
    ├── Uart.c/.h         # UART驱动
    ├── Timer.c/.h        # 定时器驱动
    └── ch32v30x_it.c/.h  # 中断处理
```

## 主要功能

### 1. 四通道称重
- 使用ADS1234芯片进行24位ADC称重
- 支持串扰补偿
- 自动校零功能

### 2. K230物品识别
- 通过UART3接收K230数据
- 支持JSON格式解析
- 实时物品位置识别

### 3. 位置匹配
```
通道1 <-> top_left (左上)
通道2 <-> top_right (右上)
通道3 <-> bottom_left (左下)
通道4 <-> bottom_right (右下)
```

### 4. 多串口通信
- UART1: 调试输出 + 串口屏
- UART3: K230通信
- UART4: PC通信（JSON数据输出）

## 硬件连接

### ADS1234称重芯片
- SCLK  → PA0
- DOUT  → PA1
- PDWN  → PB0
- A0    → PB2
- A1    → PB5

### K230模块 (UART3)
- TX    → PB11
- RX    → PB10

### 串口屏 (UART1)
- RX    → PA9
- TX    → PA10

### PC通信 (UART4)
- RX    → PC10
- TX    → PC11

## K230数据格式

```json
{
  "top_left": {"item": "apple", "confidence": 0.95},
  "top_right": {"item": "banana", "confidence": 0.87},
  "bottom_left": {"item": "orange", "confidence": 0.92},
  "bottom_right": {}
}
```

## 输出数据格式

### UART4输出到PC（JSON格式）
```json
{
  "merged_data": [
    {"position": "top_left", "item": "apple", "confidence": 0.95, "weight": 123.45},
    {"position": "top_right", "item": "banana", "confidence": 0.87, "weight": 0.00},
    {"position": "bottom_left", "item": "orange", "confidence": 0.92, "weight": 87.32},
    {"position": "bottom_right", "item": "无物品", "confidence": 0.00, "weight": 0.00}
  ],
  "total_weight": 210.77,
  "detected_count": 3
}
```

## 使用步骤

1. **打开项目**
   - 启动MounRiver IDE
   - File → Open Projects from File System...
   - 选择项目目录：`D:/BaiduNetdiskDownload/Project_MounRiver/Project/1`

2. **编译项目**
   - 右键点击项目"1"
   - Build Project

3. **下载程序**
   - 连接WCH-Link调试器
   - 点击下载按钮

4. **调试**
   - F11开始调试
   - 查看串口输出

## 校准系数

在main.c中修改校准系数（第830行左右）：
```c
g_scale_factor[0] = 0.000674763f;  // 通道1
g_scale_factor[1] = 0.000666666f;  // 通道2
g_scale_factor[2] = 0.00125f;      // 通道3
g_scale_factor[3] = 0.000366568f;  // 通道4
```

## 注意事项

1. 确保所有设备共地
2. K230发送数据格式必须符合JSON规范
3. UART波特率统一为115200
4. 首次使用需要执行自动校零

## 故障排除

### 问题1：编译错误
- 检查项目路径是否正确
- 确认工具链是否安装

### 问题2：串口无输出
- 检查引脚连接
- 确认波特率设置

### 问题3：称重数据不准
- 重新执行校零
- 调整校准系数

---
*项目创建日期: 2026-05-21*
*版本: V4.0*
