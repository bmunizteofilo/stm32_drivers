/*
 * core_m0.h
 *
 *  Created on: Aug 14, 2025
 *      Author: Bruno
 */

#ifndef __CORE_M0_H__
#define __CORE_M0_H__

/* NVIC (Cortex-M0) — acesso direto, sem helpers */
#define NVIC_ISER           (*(volatile uint32_t*)0xE000E100UL)
#define NVIC_ICER           (*(volatile uint32_t*)0xE000E180UL)
#define NVIC_IPR_BASE       ((volatile uint8_t*)0xE000E400UL) // 8-bit per IRQ on M0

/* ===== SysTick (ARMv6-M) ===== */
#define SYST_CSR   (*(volatile uint32_t*)0xE000E010UL) /* CTRL */
#define SYST_RVR   (*(volatile uint32_t*)0xE000E014UL) /* LOAD (24 bits) */
#define SYST_CVR   (*(volatile uint32_t*)0xE000E018UL) /* VAL (write any clears) */
#define SYST_CALIB (*(volatile uint32_t*)0xE000E01CUL) /* CALIB (opcional) */

/* CTRL bits */
#define SYST_CSR_ENABLE      (1u << 0)   /* contador ON */
#define SYST_CSR_TICKINT     (1u << 1)   /* IRQ ON */
#define SYST_CSR_CLKSOURCE   (1u << 2)   /* 1=AHB, 0=AHB/8 */
#define SYST_CSR_COUNTFLAG   (1u << 16)  /* zera ao ler */

/* ===== System Handler Priority Register 3 (Cortex-M0) =====
   SysTick (exceção 15) usa o byte [31:24] de SHPR3 (0xE000ED20). */
#define SCB_SHPR3 (*(volatile uint32_t*)0xE000ED20UL)

typedef enum {
    WWDG_IRQn                     = 0,
    PVD_VDDIO2_IRQn               = 1,
    RTC_IRQn                      = 2,
    FLASH_IRQn                    = 3,
    RCC_IRQn                      = 4,
    EXTI0_1_IRQn                  = 5,
    EXTI2_3_IRQn                  = 6,
    EXTI4_15_IRQn                 = 7,
    TSC_IRQn                      = 8,
    DMA1_Channel1_IRQn            = 9,
    DMA1_Channel2_3_IRQn          = 10,
    DMA1_Channel4_5_IRQn          = 11,
    ADC1_COMP_IRQn                = 12,
    TIM1_BRK_UP_TRG_COM_IRQn      = 13,
    TIM1_CC_IRQn                  = 14,
    TIM3_IRQn                     = 16,
    TIM6_DAC_IRQn                 = 17,
    TIM7_IRQn                     = 18,
    TIM14_IRQn                    = 19,
    TIM15_IRQn                    = 20,
    TIM16_IRQn                    = 21,
    TIM17_IRQn                    = 22,
    I2C1_IRQn                     = 23,
    I2C2_IRQn                     = 24,
    SPI1_IRQn                     = 25,
    SPI2_IRQn                     = 26,
    USART1_IRQn                   = 27,
    USART2_IRQn                   = 28,
    CEC_CAN_IRQn                  = 30,
    USB_IRQn                      = 31
} IRQn_Type;

/* ======= NVIC direto ======= */
static inline void nvic_enable_irq(IRQn_Type irqn, uint8_t priority) {
    /* Cortex-M0 usa apenas os 2 MSBs do campo de prioridade (0..3 efetivo) */
    NVIC_IPR_BASE[(uint32_t)irqn] = (uint8_t)(priority << 6);
    NVIC_ISER = (1u << ((uint32_t)irqn & 0x1F));
}

static inline void nvic_disable_irq(IRQn_Type irqn) {
    NVIC_ICER = (1u << ((uint32_t)irqn & 0x1F));
}

#endif /* __CORE_M0_H__ */
