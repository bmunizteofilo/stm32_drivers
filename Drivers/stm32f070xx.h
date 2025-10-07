/*
 * stm32f070xx.h
 *
 *  Created on: Aug 14, 2025
 *      Author: Bruno
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef __STM32F070XX_H__
#define __STM32F070XX_H__

#include "core_m0.h"
#include "dma_router.h"

/* ================= Base addresses essenciais ================= */
#define PERIPH_BASE        0x40000000UL
#define AHBPERIPH_BASE     (PERIPH_BASE + 0x00020000UL)
#define APB2PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)
#define APB1PERIPH_BASE    (PERIPH_BASE + 0x00000000UL)

/* ---------------- RCC ---------------- */
#define RCC_BASE           (AHBPERIPH_BASE + 0x1000UL)
typedef struct {
    volatile uint32_t CR;        // 0x00
    volatile uint32_t CFGR;      // 0x04
    volatile uint32_t CIR;       // 0x08
    volatile uint32_t APB2RSTR;  // 0x0C
    volatile uint32_t APB1RSTR;  // 0x10
    volatile uint32_t AHBENR;    // 0x14
    volatile uint32_t APB2ENR;   // 0x18
    volatile uint32_t APB1ENR;   // 0x1C
    volatile uint32_t BDCR;      // 0x20
    volatile uint32_t CSR;       // 0x24
    volatile uint32_t AHBRSTR;   // 0x28
    volatile uint32_t CFGR2;     // 0x2C (PREDIV etc.)
    volatile uint32_t CFGR3;     // 0x30
    volatile uint32_t CR2;       // 0x34
} RCC_TypeDef;

#define RCC ((RCC_TypeDef*)RCC_BASE)

/* ====== ADIÇÕES PARA LSI/LSE/RTC E PRESETS ====== */

/* PWR (para liberar acesso ao domínio de backup – BDCR) */
#define PWR_BASE           (APB1PERIPH_BASE + 0x7000UL)
typedef struct {
    volatile uint32_t CR;   // 0x00
    volatile uint32_t CSR;  // 0x04
} PWR_TypeDef;
#define PWR ((PWR_TypeDef*)PWR_BASE)

/* Bits CR */
#define RCC_CR_HSION       (1u << 0)
#define RCC_CR_HSIRDY      (1u << 1)
#define RCC_CR_HSEON       (1u << 16)
#define RCC_CR_HSERDY      (1u << 17)
#define RCC_CR_HSEBYP      (1u << 18)
#define RCC_CR_CSSON       (1u << 19)
#define RCC_CR_PLLON       (1u << 24)
#define RCC_CR_PLLRDY      (1u << 25)

/* CFGR: SW[1:0], SWS[1:0], HPRE[7:4], PPRE[10:8], ADCPRE[14], PLLSRC[16], PLLXTPRE[17], PLLMUL[21:18] */
#define RCC_CFGR_SW_Pos    0u
#define RCC_CFGR_SW_Msk    (3u << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SWS_Pos   2u
#define RCC_CFGR_SWS_Msk   (3u << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SW_HSI    (0u << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SW_HSE    (1u << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SW_PLL    (2u << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SWS_HSI   (0u << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_HSE   (1u << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_PLL   (2u << RCC_CFGR_SWS_Pos)

#define RCC_CFGR_HPRE_Pos  4u
#define RCC_CFGR_HPRE_Msk  (0xFu << RCC_CFGR_HPRE_Pos)
#define RCC_CFGR_PPRE_Pos  8u
#define RCC_CFGR_PPRE_Msk  (7u << RCC_CFGR_PPRE_Pos)

#define RCC_CFGR_PLLSRC    (1u << 16)   // 0: HSI/2 ; 1: HSE/PREDIV
#define RCC_CFGR_PLLXTPRE  (1u << 17)   // Em alguns F0, não usado; PREDIV em CFGR2 governa
#define RCC_CFGR_PLLMUL_Pos 18u
#define RCC_CFGR_PLLMUL_Msk (0xFu << RCC_CFGR_PLLMUL_Pos) // 0000:x2 ... 1111:x16

/* CFGR2 (PREDIV): bits [3:0] => /1..../16 */
#define RCC_CFGR2_PREDIV_Pos 0u
#define RCC_CFGR2_PREDIV_Msk (0xFu << RCC_CFGR2_PREDIV_Pos)

/* Bits PWR */
#define RCC_APB1ENR_PWREN  (1u << 28)  /* RCC->APB1ENR */
#define PWR_CR_DBP         (1u << 8)   /* acesso BDCR */

/* LSI / LSE em RCC */
#define RCC_CSR_LSION      (1u << 0)
#define RCC_CSR_LSIRDY     (1u << 1)

#define RCC_BDCR_LSEON     (1u << 0)
#define RCC_BDCR_LSERDY    (1u << 1)
#define RCC_BDCR_LSEBYP    (1u << 2)
#define RCC_BDCR_RTCSEL_Pos 8
#define RCC_BDCR_RTCSEL_Msk (3u << RCC_BDCR_RTCSEL_Pos)
#define RCC_BDCR_RTCEN     (1u << 15)

/* Bits de clock p/ GPIO (RCC->AHBENR) — STM32F070 */
#define RCC_AHBENR_IOPAEN   (1u << 17)
#define RCC_AHBENR_IOPBEN   (1u << 18)
#define RCC_AHBENR_IOPCEN   (1u << 19)
#define RCC_AHBENR_IOPDEN   (1u << 20)
#define RCC_AHBENR_IOPFEN   (1u << 22)

#define RCC_APB2ENR_SPI1EN  (1u<<12)
#define RCC_APB1ENR_SPI2EN  (1u<<14)

#define RCC_APB2ENR_TIM1EN   (1u<<11)
#define RCC_APB2ENR_TIM16EN  (1u<<17)
#define RCC_APB2ENR_TIM17EN  (1u<<18)
#define RCC_APB1ENR_TIM3EN   (1u<<1)
#define RCC_APB1ENR_TIM14EN  (1u<<8)

#define RCC_APB1ENR_I2C1EN   (1u<<21)
#define RCC_APB1ENR_I2C2EN   (1u<<22)

#define RCC_APB1ENR_WWDGEN   (1u << 11)

/* ---------------- FLASH ---------------- */
#define FLASH_R_BASE       0x40022000UL
typedef struct {
    volatile uint32_t ACR;     // 0x00
    volatile uint32_t KEYR;    // 0x04
    volatile uint32_t OPTKEYR; // 0x08
    volatile uint32_t SR;      // 0x0C
    volatile uint32_t CR;      // 0x10
    volatile uint32_t AR;      // 0x14
    volatile uint32_t RESERVED;
    volatile uint32_t OBR;     // 0x1C
    volatile uint32_t WRPR;    // 0x20
} FLASH_TypeDef;
#define FLASH ((FLASH_TypeDef*)FLASH_R_BASE)
#define FLASH_ACR_LATENCY  (1u << 0) // 0WS=0 ; 1WS=1
#define FLASH_ACR_PRFTBE   (1u << 4) // Prefetch enable (opcional, mas comum)


/* ================== MAPA DE REGISTRADORES BÁSICOS ================== */

/* GPIO base (STM32F070) */
#define GPIOA_BASE         (0x48000000UL)
#define GPIOB_BASE         (0x48000400UL)
#define GPIOC_BASE         (0x48000800UL)
#define GPIOD_BASE         (0x48000C00UL)
#define GPIOF_BASE         (0x48001400UL)

typedef struct {
    volatile uint32_t MODER;    // 0x00
    volatile uint32_t OTYPER;   // 0x04
    volatile uint32_t OSPEEDR;  // 0x08
    volatile uint32_t PUPDR;    // 0x0C
    volatile uint32_t IDR;      // 0x10
    volatile uint32_t ODR;      // 0x14
    volatile uint32_t BSRR;     // 0x18
    volatile uint32_t LCKR;     // 0x1C
    volatile uint32_t AFR[2];   // 0x20 (AFRL), 0x24 (AFRH)
    volatile uint32_t BRR;      // 0x28
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef*)GPIOA_BASE)
#define GPIOB ((GPIO_TypeDef*)GPIOB_BASE)
#define GPIOC ((GPIO_TypeDef*)GPIOC_BASE)
#define GPIOD ((GPIO_TypeDef*)GPIOD_BASE)
#define GPIOF ((GPIO_TypeDef*)GPIOF_BASE)


/* SYSCFG (para EXTI) */
#define SYSCFG_BASE        (APB2PERIPH_BASE + 0x0000UL)
typedef struct {
    volatile uint32_t CFGR1;     // 0x00
    volatile uint32_t RCR;       // 0x04
    volatile uint32_t EXTICR[4]; // 0x08..0x14
    volatile uint32_t CFGR2;     // 0x18
} SYSCFG_TypeDef;

#define SYSCFG ((SYSCFG_TypeDef*)SYSCFG_BASE)
#define RCC_APB2ENR_SYSCFGCOMPEN (1u << 0)

/* EXTI */
#define EXTI_BASE          (APB2PERIPH_BASE + 0x0400UL)
typedef struct {
    volatile uint32_t IMR;    // 0x00
    volatile uint32_t EMR;    // 0x04
    volatile uint32_t RTSR;   // 0x08
    volatile uint32_t FTSR;   // 0x0C
    volatile uint32_t SWIER;  // 0x10
    volatile uint32_t PR;     // 0x14
} EXTI_TypeDef;

#define EXTI ((EXTI_TypeDef*)EXTI_BASE)

/* ---------------- ADC1 (STM32F070) ---------------- */
#define ADC1_BASE          (APB2PERIPH_BASE + 0x2400UL)

typedef struct {
    volatile uint32_t ISR;      /* 0x00 */
    volatile uint32_t IER;      /* 0x04 */
    volatile uint32_t CR;       /* 0x08 */
    volatile uint32_t CFGR1;    /* 0x0C */
    volatile uint32_t CFGR2;    /* 0x10 */
    volatile uint32_t SMPR;     /* 0x14 */
    volatile uint32_t RESERVED0;/* 0x18 */
    volatile uint32_t RESERVED1;/* 0x1C */
    volatile uint32_t TR;       /* 0x20 */
    volatile uint32_t RESERVED2;/* 0x24 */
    volatile uint32_t CHSELR;   /* 0x28 */
    volatile uint32_t RESERVED3[5]; /* 0x2C,0x30,0x34,0x38,0x3C */
    volatile uint32_t DR;       /* 0x40 */
} ADC_TypeDef;


#define ADC1 ((ADC_TypeDef*)ADC1_BASE)

/* --- RCC bits específicos para ADC / HSI14 --- */
#define RCC_APB2ENR_ADCEN      (1u << 9)  /* Enable clock for ADC */
#define RCC_CR2_HSI14ON        (1u << 0)  /* HSI14 oscillator enable (ADC clock) */
#define RCC_CR2_HSI14RDY       (1u << 1)  /* HSI14 ready flag */
/* (Opcional) RCC_CR2_HSI14DIS = bit 2 em alguns F0 — não é necessário pro driver por polling */

/* --- ADC_ISR (write-1-to-clear) --- */
#define ADC_ISR_ADRDY          (1u << 0)  /* ADC ready */
#define ADC_ISR_EOSMP          (1u << 1)  /* End of sampling */
#define ADC_ISR_EOC            (1u << 2)  /* End of conversion */
#define ADC_ISR_EOS            (1u << 3)  /* End of sequence */
#define ADC_ISR_OVR            (1u << 4)  /* Overrun */
#define ADC_ISR_AWD            (1u << 7)  /* Analog watchdog */

/* --- ADC_IER --- */
#define ADC_IER_ADRDYIE        (1u << 0)
#define ADC_IER_EOSMPIE        (1u << 1)
#define ADC_IER_EOCIE          (1u << 2)
#define ADC_IER_EOSIE          (1u << 3)
#define ADC_IER_OVRIE          (1u << 4)
#define ADC_IER_AWDIE          (1u << 7)

/* --- ADC_CR --- */
#define ADC_CR_ADEN            (1u << 0)   /* ADC enable */
#define ADC_CR_ADDIS           (1u << 1)   /* ADC disable */
#define ADC_CR_ADSTART         (1u << 2)   /* Start conversion */
#define ADC_CR_ADSTP           (1u << 4)   /* Stop conversion */
#define ADC_CR_ADCAL           (1u << 31)  /* Start calibration */

/* --- ADC_CFGR1 (apenas campos úteis no bare-metal por polling) --- */
#define ADC_CFGR1_DMAEN        (1u << 0)   /* (para futuro DMA) */
#define ADC_CFGR1_DMACFG       (1u << 1)   /* 0=one-shot, 1=circular (DMA)   */
#define ADC_CFGR1_SCANDIR      (1u << 2)   /* 0=ascendente, 1=descendente    */
#define ADC_CFGR1_RES_Pos      3           /* RES[1:0]: 00=12b,01=10b,10=8b,11=6b */
#define ADC_CFGR1_RES_Msk      (3u << ADC_CFGR1_RES_Pos)
#define ADC_CFGR1_ALIGN        (1u << 5)   /* 0=right, 1=left */
#define ADC_CFGR1_EXTSEL_Pos   6           /* EXTSEL/EXTEN dependem do modelo; evitaremos uso */
#define ADC_CFGR1_EXTSEL_Msk   (7u << ADC_CFGR1_EXTSEL_Pos)
#define ADC_CFGR1_EXTEN_Pos    10
#define ADC_CFGR1_EXTEN_Msk    (3u << ADC_CFGR1_EXTEN_Pos)
#define ADC_CFGR1_OVRMOD       (1u << 12)  /* Overrun mode (0=preserve,1=overwrite) */
#define ADC_CFGR1_CONT         (1u << 13)  /* Continuous mode */
#define ADC_CFGR1_WAIT         (1u << 14)  /* Wait mode */

/* --- ADC_CFGR2 --- */
/* CKMODE[1:0]: 00 = async clock (HSI14), 01 = PCLK/2, 10 = PCLK/4 (varia entre subfamílias F0) */
#define ADC_CFGR2_CKMODE_Pos   30
#define ADC_CFGR2_CKMODE_Msk   (3u << ADC_CFGR2_CKMODE_Pos)
#define ADC_CFGR2_CKMODE_ASYNC (0u << ADC_CFGR2_CKMODE_Pos)

/* --- ADC_SMPR --- */
#define ADC_SMPR_SMP_Pos       0          /* SMP[2:0]: 1.5, 7.5, 13.5, 28.5, 41.5, 55.5, 71.5, 239.5 cycles */
#define ADC_SMPR_SMP_Msk       (7u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_1C5           (0u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_7C5           (1u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_13C5          (2u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_28C5          (3u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_41C5          (4u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_55C5          (5u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_71C5          (6u << ADC_SMPR_SMP_Pos)
#define ADC_SMPR_239C5         (7u << ADC_SMPR_SMP_Pos)

/* --- ADC_TR (watchdog) --- */
#define ADC_TR_LT_Pos          0
#define ADC_TR_LT_Msk          (0xFFFu << ADC_TR_LT_Pos)
#define ADC_TR_HT_Pos          16
#define ADC_TR_HT_Msk          (0xFFFu << ADC_TR_HT_Pos)

/* --- ADC_CHSELR (seleção de canais “one-hot”) --- */
#define ADC_CHSELR_CH(n)       (1u << (n))   /* n = 0..18 (depende do pacote) */

/* Mapeamento típico STM32F070:
   - IN0..IN7  -> PA0..PA7
   - IN8..IN9  -> PB0..PB1
   - IN10..IN15-> PC0..PC5 (se disponíveis)
   - IN16 = VREFINT
   - IN17 = TS (sensor de temperatura)
   - IN18 = VBAT (se suportado/configurado) */

/* --- ADC_DR --- */
#define ADC_DR_DATA_Msk        0xFFFFu      /* leitura de 12b alinhada conforme CFGR1.ALIGN */

/* ===== ADC extras (F0) ===== */

/* Resoluções em CFGR1.RES[1:0] */
#define ADC_CFGR1_RES_12BIT   (0u << ADC_CFGR1_RES_Pos)
#define ADC_CFGR1_RES_10BIT   (1u << ADC_CFGR1_RES_Pos)
#define ADC_CFGR1_RES_8BIT    (2u << ADC_CFGR1_RES_Pos)
#define ADC_CFGR1_RES_6BIT    (3u << ADC_CFGR1_RES_Pos)

/* Modos adicionais CFGR1 */
#define ADC_CFGR1_AUTOFF      (1u << 15)   /* Auto-off após cada conversão */
#define ADC_CFGR1_DISCEN      (1u << 16)   /* Discontinuous mode */

/* CFGR2 CKMODE (clock síncrono derivado do PCLK) */
#define ADC_CFGR2_CKMODE_PCLK_DIV2  (1u << ADC_CFGR2_CKMODE_Pos)
#define ADC_CFGR2_CKMODE_PCLK_DIV4  (2u << ADC_CFGR2_CKMODE_Pos)

/* EXTEN edges (CFGR1.EXTEN[1:0]) */
#define ADC_EXTEN_DISABLED    (0u << ADC_CFGR1_EXTEN_Pos)
#define ADC_EXTEN_RISING      (1u << ADC_CFGR1_EXTEN_Pos)
#define ADC_EXTEN_FALLING     (2u << ADC_CFGR1_EXTEN_Pos)
#define ADC_EXTEN_BOTH        (3u << ADC_CFGR1_EXTEN_Pos)

/* Nota: EXTSEL é dependente do modelo (3 bits). Usaremos valor cru informado na config. */



#define RCC_APB2ENR_USART1EN   (1u<<14)
#define RCC_APB1ENR_USART2EN   (1u<<17)

/* USART (linha F0) */

#define USART1_BASE        (APB2PERIPH_BASE + 0x3800UL)
#define USART2_BASE        (APB1PERIPH_BASE + 0x4400UL)
typedef struct {
    volatile uint32_t CR1;   // 0x00
    volatile uint32_t CR2;   // 0x04
    volatile uint32_t CR3;   // 0x08
    volatile uint32_t BRR;   // 0x0C
    volatile uint32_t GTPR;  // 0x10
    volatile uint32_t RTOR;  // 0x14
    volatile uint32_t RQR;   // 0x18
    volatile uint32_t ISR;   // 0x1C
    volatile uint32_t ICR;   // 0x20
    volatile uint32_t RDR;   // 0x24
    volatile uint32_t TDR;   // 0x28
} USART_TypeDef;
#define USART1 ((USART_TypeDef*)USART1_BASE)
#define USART2 ((USART_TypeDef*)USART2_BASE)


/* DMA1 (F0) */
#define DMA1_BASE          (PERIPH_BASE + 0x00020000UL)
typedef struct {
	volatile uint32_t CCR, CNDTR, CPAR, CMAR, RESERVED;
} DMA_Channel_TypeDef;
typedef struct {
	volatile uint32_t ISR, IFCR;
	DMA_Channel_TypeDef CH[7];
} DMA_TypeDef;
#define DMA1 ((DMA_TypeDef*)DMA1_BASE)
#define DMA1_Channel1 (&DMA1->CH[0])
#define DMA1_Channel2 (&DMA1->CH[1])
#define DMA1_Channel3 (&DMA1->CH[2])
#define DMA1_Channel4 (&DMA1->CH[3])
#define DMA1_Channel5 (&DMA1->CH[4])
#define DMA1_Channel6 (&DMA1->CH[5])
#define DMA1_Channel7 (&DMA1->CH[6])

#define RCC_AHBENR_DMA1EN    (1u<<0)

/* DMA bits/flags */
#define DMA_CCR_EN        (1u<<0)
#define DMA_CCR_TCIE      (1u<<1)
#define DMA_CCR_HTIE      (1u<<2)
#define DMA_CCR_TEIE      (1u<<3)
#define DMA_CCR_DIR       (1u<<4) /* 1: mem->periph */
#define DMA_CCR_CIRC      (1u<<5)
#define DMA_CCR_PINC      (1u<<6)
#define DMA_CCR_MINC      (1u<<7)
#define DMA_CCR_PSIZE_8   (0u<<8)
#define DMA_CCR_PSIZE_16  (1u<<8)
#define DMA_CCR_MSIZE_8   (0u<<10)
#define DMA_CCR_MSIZE_16  (1u<<10)
#define DMA_CCR_PL_HIGH   (2u<<12)
#define DMA_CCR_PSIZE_Pos  8         /* 0:8-bit, 1:16, 2:32 */
#define DMA_CCR_MSIZE_Pos  10
#define DMA_CCR_PL_Pos     12        /* priority 0..3 */
#define DMA_CCR_MEM2MEM    (1u<<14)

/* Flags por canal n (1..5) para seu dispatcher */
#define DMA_GIF(n)   (1u << (4*((n)-1) + 0))
#define DMA_TCIF(n)  (1u << (4*((n)-1) + 1))
#define DMA_HTIF(n)  (1u << (4*((n)-1) + 2))
#define DMA_TEIF(n)  (1u << (4*((n)-1) + 3))

/* Macros “nominais” (compatíveis com seu código atual) */
#define DMA_TCIF1 DMA_TCIF(1)
#define DMA_HTIF1 DMA_HTIF(1)
#define DMA_TEIF1 DMA_TEIF(1)
#define DMA_TCIF2 DMA_TCIF(2)
#define DMA_HTIF2 DMA_HTIF(2)
#define DMA_TEIF2 DMA_TEIF(2)
#define DMA_TCIF3 DMA_TCIF(3)
#define DMA_HTIF3 DMA_HTIF(3)
#define DMA_TEIF3 DMA_TEIF(3)
#define DMA_TCIF4 DMA_TCIF(4)
#define DMA_HTIF4 DMA_HTIF(4)
#define DMA_TEIF4 DMA_TEIF(4)
#define DMA_TCIF5 DMA_TCIF(5)
#define DMA_HTIF5 DMA_HTIF(5)
#define DMA_TEIF5 DMA_TEIF(5)

#define SPI1_BASE          (APB2PERIPH_BASE + 0x3000UL) /* 0x40013000 */
#define SPI2_BASE          (APB1PERIPH_BASE + 0x3800UL) /* 0x40003800 */
typedef struct {
  volatile uint32_t CR1;     /* 0x00 */
  volatile uint32_t CR2;     /* 0x04 */
  volatile uint32_t SR;      /* 0x08 */
  volatile uint32_t DR;      /* 0x0C (acesso 8/16 bits) */
  volatile uint32_t CRCPR;   /* 0x10 */
  volatile uint32_t RXCRCR;  /* 0x14 */
  volatile uint32_t TXCRCR;  /* 0x18 */
  volatile uint32_t I2SCFGR; /* 0x1C */
  volatile uint32_t I2SPR;   /* 0x20 */
} SPI_TypeDef;
#define SPI1 ((SPI_TypeDef*)SPI1_BASE)
#define SPI2 ((SPI_TypeDef*)SPI2_BASE)

/* ---------------- TIM (layout comum F0) ---------------- */
typedef struct {
  volatile uint32_t CR1;     /* 0x00 */
  volatile uint32_t CR2;     /* 0x04 */
  volatile uint32_t SMCR;    /* 0x08 */
  volatile uint32_t DIER;    /* 0x0C */
  volatile uint32_t SR;      /* 0x10 */
  volatile uint32_t EGR;     /* 0x14 */
  volatile uint32_t CCMR1;   /* 0x18 */
  volatile uint32_t CCMR2;   /* 0x1C */
  volatile uint32_t CCER;    /* 0x20 */
  volatile uint32_t CNT;     /* 0x24 */
  volatile uint32_t PSC;     /* 0x28 */
  volatile uint32_t ARR;     /* 0x2C */
  volatile uint32_t RCR;     /* 0x30 (só TIM1) */
  volatile uint32_t CCR1;    /* 0x34 */
  volatile uint32_t CCR2;    /* 0x38 */
  volatile uint32_t CCR3;    /* 0x3C */
  volatile uint32_t CCR4;    /* 0x40 */
  volatile uint32_t BDTR;    /* 0x44 (só TIM1) */
  volatile uint32_t DCR;     /* 0x48 */
  volatile uint32_t DMAR;    /* 0x4C */
} TIM_TypeDef;

/* Bases típicas do F070 */
#define TIM1_BASE  (APB2PERIPH_BASE + 0x2C00UL)
#define TIM3_BASE  (APB1PERIPH_BASE + 0x0400UL)
#define TIM14_BASE (APB1PERIPH_BASE + 0x2000UL)
#define TIM16_BASE (APB2PERIPH_BASE + 0x3400UL)
#define TIM17_BASE (APB2PERIPH_BASE + 0x3800UL)
#define TIM1  ((TIM_TypeDef*)TIM1_BASE)
#define TIM3  ((TIM_TypeDef*)TIM3_BASE)
#define TIM14 ((TIM_TypeDef*)TIM14_BASE)
#define TIM16 ((TIM_TypeDef*)TIM16_BASE)
#define TIM17 ((TIM_TypeDef*)TIM17_BASE)

/* I2C v2 (F0/F3/L0) */
typedef struct {
  volatile uint32_t CR1;      /* 0x00 */
  volatile uint32_t CR2;      /* 0x04 */
  volatile uint32_t OAR1;     /* 0x08 */
  volatile uint32_t OAR2;     /* 0x0C */
  volatile uint32_t TIMINGR;  /* 0x10 */
  volatile uint32_t TIMEOUTR; /* 0x14 */
  volatile uint32_t ISR;      /* 0x18 */
  volatile uint32_t ICR;      /* 0x1C */
  volatile uint32_t PECR;     /* 0x20 */
  volatile uint32_t RXDR;     /* 0x24 */
  volatile uint32_t TXDR;     /* 0x28 */
} I2C_TypeDef;

#define I2C1_BASE  (APB1PERIPH_BASE + 0x5400UL)
#define I2C2_BASE  (APB1PERIPH_BASE + 0x5800UL)
#define I2C1       ((I2C_TypeDef*)I2C1_BASE)
#define I2C2       ((I2C_TypeDef*)I2C2_BASE)

/* I2C CR1 interrupt bits – posições corretas no STM32F0 */
#define I2C_CR1_TXIE      (1u<<1)
#define I2C_CR1_RXIE      (1u<<2)
#define I2C_CR1_ADDRIE    (1u<<3)
#define I2C_CR1_NACKIE    (1u<<4)
#define I2C_CR1_STOPIE    (1u<<5)
#define I2C_CR1_TCIE      (1u<<6)
#define I2C_CR1_ERRIE     (1u<<7)
#define I2C_CR1_TxDMAEN   (1u<<14)
#define I2C_CR1_RxDMAEN   (1u<<15)

/* CR1 bits */
#define I2C_CR1_PE          (1u<<0)
#define I2C_CR1_ANFOFF      (1u<<12) /* 0 = filtro analógico ON; 1 = OFF */
#define I2C_CR1_DNF_Pos     8        /* filtro digital (0..15) */
#define I2C_CR1_DNF_Msk     (0xFu<<I2C_CR1_DNF_Pos)

/* CR2 fields (7-bit): SADD[9:0], RD_WRN[10], START[13], STOP[14], NACK[15], NBYTES[23:16], RELOAD[24], AUTOEND[25] */
#define I2C_CR2_SADD_7(a7)  ((uint32_t)((a7) & 0x7Fu) << 1)
#define I2C_CR2_RD_WRN      (1u<<10)
#define I2C_CR2_ADD10       (1u<<11)
#define I2C_CR2_START       (1u<<13)
#define I2C_CR2_STOP        (1u<<14)
#define I2C_CR2_NACK        (1u<<15)
#define I2C_CR2_NBYTES(n)   ((uint32_t)((n) & 0xFFu) << 16)
#define I2C_CR2_RELOAD      (1u<<24)
#define I2C_CR2_AUTOEND     (1u<<25)

/* ISR bits */
#define I2C_ISR_TXE         (1u<<0)
#define I2C_ISR_TXIS        (1u<<1)
#define I2C_ISR_RXNE        (1u<<2)
#define I2C_ISR_ADDR        (1u<<3)
#define I2C_ISR_NACKF       (1u<<4)
#define I2C_ISR_STOPF       (1u<<5)
#define I2C_ISR_TC          (1u<<6)
#define I2C_ISR_TCR         (1u<<7)
#define I2C_ISR_BERR        (1u<<8)
#define I2C_ISR_ARLO        (1u<<9)
#define I2C_ISR_OVR         (1u<<10)
#define I2C_ISR_TIMEOUT     (1u<<12)
#define I2C_ISR_BUSY        (1u<<15)

/* ICR bits (write-1-to-clear) */
#define I2C_ICR_ADDRCF      (1u<<3)
#define I2C_ICR_NACKCF      (1u<<4)
#define I2C_ICR_STOPCF      (1u<<5)
#define I2C_ICR_BERRCF      (1u<<8)
#define I2C_ICR_ARLOCF      (1u<<9)
#define I2C_ICR_OVRCF       (1u<<10)
#define I2C_ICR_TIMOUTCF    (1u<<12)

/* TIMINGR presets (comuns para PCLK1=48MHz; ajuste se necessário)
   Estes são valores típicos encontrados em exemplos de F0:
   - 100 kHz: PRESC=3, SCLL=0x13, SCLH=0xF, SDADEL=0x2, SCLDEL=0x4 → 0x00303D5B
   - 400 kHz: PRESC=1, SCLL=0x9,  SCLH=0x3, SDADEL=0x2, SCLDEL=0x3 → 0x00100106
   Se o seu barramento não reconhecer, forneça TIMINGR explicitamente via init. */
#define I2C_TIMINGR_100K_48M  0x00303D5BUL
#define I2C_TIMINGR_400K_48M  0x00100106UL

/* ===== WWDG (Window Watchdog) ===== */
#define WWDG_BASE   (APB1PERIPH_BASE + 0x2C00UL)
typedef struct {
    volatile uint32_t CR;   /* 0x00: [6:0] T, [7] WDGA */
    volatile uint32_t CFR;  /* 0x04: [6:0] W, [8:7] WDGTB, [9] EWI */
    volatile uint32_t SR;   /* 0x08: [0] EWIF */
} WWDG_TypeDef;
#define WWDG ((WWDG_TypeDef*)WWDG_BASE)

/* WWDG bits/fields */
#define WWDG_CR_T_Msk       0x7Fu
#define WWDG_CR_WDGA        (1u<<7)

#define WWDG_CFR_W_Msk      0x7Fu
#define WWDG_CFR_WDGTB_Pos  7u
#define WWDG_CFR_WDGTB_Msk  (3u<<WWDG_CFR_WDGTB_Pos) /* 00:/1, 01:/2, 10:/4, 11:/8 */
#define WWDG_CFR_EWI        (1u<<9)

#define WWDG_SR_EWIF        (1u<<0)

/* ===== IWDG (Independent Watchdog) ===== */
#define IWDG_BASE  (APB1PERIPH_BASE + 0x3000UL)
typedef struct {
    volatile uint32_t KR;    /* 0x00: keys */
    volatile uint32_t PR;    /* 0x04: prescaler */
    volatile uint32_t RLR;   /* 0x08: reload (12b) */
    volatile uint32_t SR;    /* 0x0C: status: [0]PVU,[1]RVU,[2]WVU */
    volatile uint32_t WINR;  /* 0x10: window (12b) */
} IWDG_TypeDef;
#define IWDG ((IWDG_TypeDef*)IWDG_BASE)

/* IWDG keys */
#define IWDG_KR_UNLOCK   0x5555u
#define IWDG_KR_RELOAD   0xAAAAu
#define IWDG_KR_START    0xCCCCu

/* IWDG PR prescalers (PR[2:0]) → 4..256 */
#define IWDG_PR_DIV4     0u
#define IWDG_PR_DIV8     1u
#define IWDG_PR_DIV16    2u
#define IWDG_PR_DIV32    3u
#define IWDG_PR_DIV64    4u
#define IWDG_PR_DIV128   5u
#define IWDG_PR_DIV256   6u  /* 7 é reservado em muitos F0 */

/* Para STM32F070, o SR só tem PVU e RVU */
#define IWDG_SR_PVU      (1u<<0)
#define IWDG_SR_RVU      (1u<<1)

#ifndef DBGMCU_BASE
#define DBGMCU_BASE (0x40015800UL)
#endif
typedef struct {
    volatile uint32_t IDCODE; /* 0x00 */
    volatile uint32_t CR;     /* 0x04 */
    volatile uint32_t APB1FZ; /* 0x08 */
    volatile uint32_t APB2FZ; /* 0x0C */
} DBGMCU_TypeDef;
#define DBGMCU ((DBGMCU_TypeDef*)DBGMCU_BASE)

/* Bits típicos do F0 p/ freeze em debug (ajuste se seu RM divergir) */
#ifndef DBGMCU_APB1_FZ_DBG_WWDG_STOP
#define DBGMCU_APB1_FZ_DBG_WWDG_STOP (1u<<11)
#endif
#ifndef DBGMCU_APB1_FZ_DBG_IWDG_STOP
#define DBGMCU_APB1_FZ_DBG_IWDG_STOP (1u<<12)
#endif

#endif /* __STM32F070XX_H__ */
