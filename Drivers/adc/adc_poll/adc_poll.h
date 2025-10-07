#ifndef ADC_BM_H
#define ADC_BM_H

#include "stm32f070xx.h"


/* ========= Resolução / alinhamento / scan ========= */
typedef enum {
  ADC_BM_RES_12 = (0u << ADC_CFGR1_RES_Pos),
  ADC_BM_RES_10 = (1u << ADC_CFGR1_RES_Pos),
  ADC_BM_RES_8  = (2u << ADC_CFGR1_RES_Pos),
  ADC_BM_RES_6  = (3u << ADC_CFGR1_RES_Pos),
} adc_bm_resolution_t;

typedef enum { ADC_BM_ALIGN_RIGHT = 0, ADC_BM_ALIGN_LEFT = 1 } adc_bm_align_t;
typedef enum { ADC_BM_SCAN_ASC = 0, ADC_BM_SCAN_DESC = 1 } adc_bm_scan_dir_t;

/* ========= Clock do ADC ========= */
typedef enum {
  ADC_BM_CLK_ASYNC_HSI14 = 0u << 30,  /* CFGR2.CKMODE = 00 (HSI14) */
  ADC_BM_CLK_PCLK_DIV2   = 1u << 30,  /* 01 */
  ADC_BM_CLK_PCLK_DIV4   = 2u << 30,  /* 10 */
} adc_bm_clk_t;

/* ========= Tempo de amostragem ========= */
typedef enum {
  ADC_BM_SMP_1C5   = ADC_SMPR_1C5,
  ADC_BM_SMP_7C5   = ADC_SMPR_7C5,
  ADC_BM_SMP_13C5  = ADC_SMPR_13C5,
  ADC_BM_SMP_28C5  = ADC_SMPR_28C5,
  ADC_BM_SMP_41C5  = ADC_SMPR_41C5,
  ADC_BM_SMP_55C5  = ADC_SMPR_55C5,
  ADC_BM_SMP_71C5  = ADC_SMPR_71C5,
  ADC_BM_SMP_239C5 = ADC_SMPR_239C5
} adc_bm_sample_time_t;

/* ========= Gatilho externo (EXTSEL/EXTEN – STM32F070) =========
   000: TIM1_TRGO
   001: TIM1_CC4
   010: TIM3_TRGO
   011: TIM15_TRGO
   1xx: reservado
*/
typedef enum {
  ADC_BM_EXTSEL_TIM1_TRGO  = (0u << ADC_CFGR1_EXTSEL_Pos),
  ADC_BM_EXTSEL_TIM1_CC4   = (1u << ADC_CFGR1_EXTSEL_Pos),
  ADC_BM_EXTSEL_TIM3_TRGO  = (2u << ADC_CFGR1_EXTSEL_Pos),
  ADC_BM_EXTSEL_TIM15_TRGO = (3u << ADC_CFGR1_EXTSEL_Pos),
} adc_bm_extsel_t;

typedef enum {
  ADC_BM_EXT_DISABLED = (0u << ADC_CFGR1_EXTEN_Pos),
  ADC_BM_EXT_RISING   = (1u << ADC_CFGR1_EXTEN_Pos),
  ADC_BM_EXT_FALLING  = (2u << ADC_CFGR1_EXTEN_Pos),
  ADC_BM_EXT_BOTH     = (3u << ADC_CFGR1_EXTEN_Pos),
} adc_bm_extedge_t;

/* ========= Config do driver ========= */
typedef struct {
  adc_bm_clk_t        clk_mode;       /* CKMODE (PCLK/2,/4 ou HSI14 async)         */
  bool                enable_hsi14;   /* se clk_mode=ASYNC */
  adc_bm_resolution_t resolution;
  adc_bm_align_t      align;
  adc_bm_scan_dir_t   scan_dir;
  adc_bm_sample_time_t sample_time;
  bool                overrun_overwrite;  /* OVRMOD=1 recomendado */
  /* DMA flags no ADC (o controlador DMA é armado nas APIs de DMA) */
  bool                dma_enable;
  bool                dma_circular;
  /* Gatilho externo */
  adc_bm_extsel_t     extsel;         /* fonte do trigger */
  adc_bm_extedge_t    extedge;        /* borda */
} adc_bm_config_t;

/* ========= API base ========= */
adc_bm_config_t adc_bm_default(void);
void adc_bm_init(const adc_bm_config_t *cfg);

/* seleção de canais */
void adc_bm_set_channels_mask(uint32_t chsel_mask);     /* bits 0..18 */
void adc_bm_set_channels_list(const uint8_t *list, uint8_t n);

/* ========= Polling =========
   - Se cfg.extedge == DISABLED: dispara por software e lê até s_ch_count.
   - Se cfg.extedge != DISABLED: NÃO dispara; aguarda a próxima sequência vinda do trigger externo. */
uint8_t  adc_bm_read_sequence_polling(uint16_t *out, uint8_t max_samples);
uint16_t adc_bm_read_single(uint8_t channel);

/* ========= Interrupção (IRQ do ADC) ========= */
typedef void (*adc_bm_it_cb_t)(uint16_t sample, uint8_t idx_in_seq, bool eos, void *ctx);
void adc_bm_it_start(uint8_t nvic_prio, bool eoc_irq, bool eos_irq, bool ovr_irq,
                     adc_bm_it_cb_t cb, void *ctx);
void adc_bm_it_stop(void);

/* ========= DMA (compatível com seu dma_router) ========= */
typedef void (*adc_bm_dma_cb_t)(uint32_t dma_flags, void *ctx);

/* One-shot: transfere 'count' amostras (16b) e dá TC (sem HT). */
bool adc_bm_dma_start_oneshot(uint16_t *dst, uint16_t count,
                              uint8_t dma_priority, bool te_irq,
                              adc_bm_dma_cb_t cb, void *ctx);

/* Circular (streaming): preenche ring continuamente; HT/TC opcionais. */
bool adc_bm_dma_start_circular(uint16_t *ring, uint16_t length,
                               uint8_t dma_priority, bool ht_irq, bool tc_irq, bool te_irq,
                               adc_bm_dma_cb_t cb, void *ctx);

void adc_bm_dma_stop(void);

/* ========= Utilidades ========= */
void  gpio_to_analog(GPIO_TypeDef *GPIOx, uint8_t pin);
int8_t adc_bm_channel_from_gpio(GPIO_TypeDef *GPIOx, uint8_t pin);

/* Handlers de compatibilidade */
void ADC_IRQHandler(void);

#endif /* ADC_BM_H */
