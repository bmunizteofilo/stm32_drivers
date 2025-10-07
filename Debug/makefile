################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Startup/subdir.mk
-include Src/subdir.mk
-include Drivers/watchdog/subdir.mk
-include Drivers/usart/usart_poll/subdir.mk
-include Drivers/usart/usart_irq_dma/subdir.mk
-include Drivers/tim/subdir.mk
-include Drivers/systick/subdir.mk
-include Drivers/spi/spi_poll/subdir.mk
-include Drivers/spi/spi_irq_dma/subdir.mk
-include Drivers/rcc/subdir.mk
-include Drivers/i2c/i2c_poll/subdir.mk
-include Drivers/i2c/i2c_irq_dma/subdir.mk
-include Drivers/gpio/subdir.mk
-include Drivers/dma/subdir.mk
-include Drivers/adc/adc_poll/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := drivers_stm32f070
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
drivers_stm32f070.elf \

MAP_FILES += \
drivers_stm32f070.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
drivers_stm32f070.list \


# All Target
all: main-build

# Main-build Target
main-build: drivers_stm32f070.elf secondary-outputs

# Tool invocations
drivers_stm32f070.elf drivers_stm32f070.map: $(OBJS) $(USER_OBJS) C:\Users\Bruno\STM32CubeIDE\Workspace_drivers_f070\drivers_stm32f070\STM32F070RBTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "drivers_stm32f070.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m0 -T"C:\Users\Bruno\STM32CubeIDE\Workspace_drivers_f070\drivers_stm32f070\STM32F070RBTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="drivers_stm32f070.map" -Wl,--gc-sections -static --specs=nano.specs -mfloat-abi=soft -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

drivers_stm32f070.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "drivers_stm32f070.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) default.size.stdout drivers_stm32f070.elf drivers_stm32f070.list drivers_stm32f070.map
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
