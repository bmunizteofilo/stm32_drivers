################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/usart/usart_irq/usart_irq.c 

OBJS += \
./Drivers/usart/usart_irq/usart_irq.o 

C_DEPS += \
./Drivers/usart/usart_irq/usart_irq.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/usart/usart_irq/%.o Drivers/usart/usart_irq/%.su Drivers/usart/usart_irq/%.cyclo: ../Drivers/usart/usart_irq/%.c Drivers/usart/usart_irq/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DSTM32F070RBTx -DSTM32 -DSTM32F0 -DNUCLEO_F070RB -c -I../Inc -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/gpio" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/rcc" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/systick" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart/usart_poll" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart/usart_irq" -I"C:/Users/Bruno/STM32CubeIDE/Workspace_drivers_f070/drivers_stm32f070/Drivers/usart/usart_dma" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Drivers-2f-usart-2f-usart_irq

clean-Drivers-2f-usart-2f-usart_irq:
	-$(RM) ./Drivers/usart/usart_irq/usart_irq.cyclo ./Drivers/usart/usart_irq/usart_irq.d ./Drivers/usart/usart_irq/usart_irq.o ./Drivers/usart/usart_irq/usart_irq.su

.PHONY: clean-Drivers-2f-usart-2f-usart_irq

