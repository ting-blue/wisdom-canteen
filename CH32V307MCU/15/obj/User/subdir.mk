################################################################################
# MRS Version: 1.9.2
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/ADS1234.c \
../User/CLoud_protocol.c \
../User/CLoude_bridge.c \
../User/Card.c \
../User/System.c \
../User/Timer.c \
../User/Uart.c \
../User/Uart1.c \
../User/ch32v30x_it.c \
../User/main.c \
../User/system_ch32v30x.c \
../User/test_uart4.c 

OBJS += \
./User/ADS1234.o \
./User/CLoud_protocol.o \
./User/CLoude_bridge.o \
./User/Card.o \
./User/System.o \
./User/Timer.o \
./User/Uart.o \
./User/Uart1.o \
./User/ch32v30x_it.o \
./User/main.o \
./User/system_ch32v30x.o \
./User/test_uart4.o 

C_DEPS += \
./User/ADS1234.d \
./User/CLoud_protocol.d \
./User/CLoude_bridge.d \
./User/Card.d \
./User/System.d \
./User/Timer.d \
./User/Uart.d \
./User/Uart1.d \
./User/ch32v30x_it.d \
./User/main.d \
./User/system_ch32v30x.d \
./User/test_uart4.d 


# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized  -g -I"D:\wx\xwechat_files\wxid_412wgtmi4s8r12_856a\msg\file\2026-06\15\15\15\15\15\15\Debug" -I"D:\wx\xwechat_files\wxid_412wgtmi4s8r12_856a\msg\file\2026-06\15\15\15\15\15\15\Core" -I"D:\wx\xwechat_files\wxid_412wgtmi4s8r12_856a\msg\file\2026-06\15\15\15\15\15\15\User" -I"D:\wx\xwechat_files\wxid_412wgtmi4s8r12_856a\msg\file\2026-06\15\15\15\15\15\15\Peripheral\inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@	@

