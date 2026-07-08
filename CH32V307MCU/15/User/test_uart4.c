/*
 * UART4测试文件
 * 将此代码复制到main.c的主循环中来测试UART4是否正常工作
 */

/* 在主循环中添加以下代码来测试UART4： */

/*
    // UART4测试代码 - 每2秒发送一次
    static uint32_t last_test = 0;
    if(GetSysTick() - last_test >= 2000)
    {
        last_test = GetSysTick();

        UART4_SendString("\r\n=== UART4 TEST ===\r\n");
        UART4_SendString("Time: ");
        char time_buf[32];
        sprintf(time_buf, "%lu\r\n", last_test / 1000);
        UART4_SendString(time_buf);
        UART4_SendString("UART4 is working!\r\n");
        UART4_SendString("==================\r\n");
    }
*/

/*
 * 如果看到以上输出，说明UART4正常工作
 * 如果没有看到，请检查：
 * 1. UART4是否正确初始化
 * 2. 串口助手是否连接到正确的端口（UART4: PC10/TX, PC11/RX）
 * 3. 波特率是否设置为115200
 * 4. TX/RX是否接反了
 */

/*
 * UART4引脚定义：
 * TX: PC10
 * RX: PC11
 * 波特率: 115200
 */