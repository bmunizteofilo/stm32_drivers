################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/tim/tim.c 

OBJS += \
./Drivers/tim/tim.o 

C_DEPS += \
./Drivers/tim/tim.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/tim/%.o Drivers/tim/%.su Drivers/tim/%.cyclo: ../Drivers/tim/%.c Drivers/tim/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DSTM32F070RBTx -DSTM32 -DSTM32F0 -DNUCLEO_F070RB -c -I../Inc -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/gpio" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/rcc" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/systick" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart/usart_poll" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart/usart_irq_dma" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/spi" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/spi/spi_poll" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/spi/spi_irq_dma" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/dma" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/tim" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/i2c" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/i2c/i2c_poll" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/i2c/i2c_irq_dma" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/adc" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/adc/adc_poll" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/watchdog" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Drivers-2f-tim

clean-Drivers-2f-tim:
	-$(RM) ./Drivers/tim/tim.cyclo ./Drivers/tim/tim.d ./Drivers/tim/tim.o ./Drivers/tim/tim.su

.PHONY: clean-Drivers-2f-tim

