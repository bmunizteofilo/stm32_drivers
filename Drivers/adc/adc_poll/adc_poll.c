#include "adc_poll.h"

#define ADC_DMA_CH 1u /* F070: ADC usa DMA1 Channel 1 */

#ifndef ADC_BM_TIMEOUT
#define ADC_BM_TIMEOUT  (2000000u)
#endif

/* ===== Estado ===== */
static uint32_t s_chsel_mask = 0;
static uint8_t  s_ch_count   = 0;

/* lembra se o init pediu trigger externo */
static adc_bm_extedge_t s_extedge_cfg = ADC_BM_EXT_DISABLED;

/* IRQ */
static adc_bm_it_cb_t s_it_cb = NULL;
static void          *s_it_ctx = NULL;
static uint8_t        s_seq_idx = 0;

/* DMA */
static adc_bm_dma_cb_t s_dma_cb  = NULL;
static void           *s_dma_ctx = NULL;

/* ===== Utils ===== */
static inline uint8_t popcount32(uint32_t x){
  x = x - ((x >> 1) & 0x55555555u);
  x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
  return (uint8_t)((((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
}

static inline void gpio_enable_clock(GPIO_TypeDef *GPIOx){
  if (GPIOx==GPIOA) RCC->AHBENR |= RCC_AHBENR_IOPAEN;
  else if (GPIOx==GPIOB) RCC->AHBENR |= RCC_AHBENR_IOPBEN;
  else if (GPIOx==GPIOC) RCC->AHBENR |= RCC_AHBENR_IOPCEN;
  else if (GPIOx==GPIOD) RCC->AHBENR |= RCC_AHBENR_IOPDEN;
  else if (GPIOx==GPIOF) RCC->AHBENR |= RCC_AHBENR_IOPFEN;
}
void gpio_to_analog(GPIO_TypeDef *GPIOx, uint8_t pin){
  gpio_enable_clock(GPIOx);
  GPIOx->MODER |=  (3u << (pin*2));
  GPIOx->PUPDR &= ~(3u << (pin*2));
}
int8_t adc_bm_channel_from_gpio(GPIO_TypeDef *GPIOx, uint8_t pin){
  if (GPIOx==GPIOA && pin<=7) return (int8_t)pin;        /* 0..7 */
  if (GPIOx==GPIOB && pin<=1) return (int8_t)(8 + pin);  /* 8..9 */
  if (GPIOx==GPIOC && pin<=5) return (int8_t)(10 + pin); /* 10..15 */
  return -1;
}

static inline uint16_t read_dr_aligned(void){
  uint32_t dr = ADC1->DR, cf = ADC1->CFGR1;
  uint32_t res = (cf & ADC_CFGR1_RES_Msk) >> ADC_CFGR1_RES_Pos;
  bool left = (cf & ADC_CFGR1_ALIGN) != 0;
  if (!left){
    switch (res){ case 0: return dr & 0x0FFF; case 1: return dr & 0x03FF;
                  case 2: return dr & 0x00FF; default: return dr & 0x003F; }
  } else {
    switch (res){ case 0: return (dr>>4)&0x0FFF; case 1: return (dr>>6)&0x03FF;
                  case 2: return (dr>>8)&0x00FF; default: return (dr>>10)&0x003F; }
  }
}

/* ===== Config ===== */
adc_bm_config_t adc_bm_default(void){
  adc_bm_config_t c;
  c.clk_mode     = ADC_BM_CLK_PCLK_DIV4;  /* evita HSI14 por padrão */
  c.enable_hsi14 = false;
  c.resolution   = ADC_BM_RES_12;
  c.align        = ADC_BM_ALIGN_RIGHT;
  c.scan_dir     = ADC_BM_SCAN_ASC;
  c.sample_time  = ADC_BM_SMP_239C5;      /* robusto */
  c.overrun_overwrite = true;             /* OVRMOD=1 */
  c.dma_enable   = false;
  c.dma_circular = false;
  c.extsel       = ADC_BM_EXTSEL_TIM1_TRGO;
  c.extedge      = ADC_BM_EXT_DISABLED;   /* por padrão, software trigger */
  return c;
}

void adc_bm_init(const adc_bm_config_t *cfg){
  /* clock do periférico ADC */
  RCC->APB2ENR |= RCC_APB2ENR_ADCEN;

  /* clock do ADC (CFGR2) */
  ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE_Msk;
  if (cfg->clk_mode == ADC_BM_CLK_ASYNC_HSI14){
    if (cfg->enable_hsi14){
      RCC->CR2 |= RCC_CR2_HSI14ON; while ((RCC->CR2 & RCC_CR2_HSI14RDY)==0) {}
    }
    /* CKMODE=00 (async) */
  } else {
    ADC1->CFGR2 |= (uint32_t)cfg->clk_mode; /* PCLK/2 ou /4 */
  }

  /* Para conversões e desabilita ADC se estiver ligado */
  if (ADC1->CR & ADC_CR_ADSTART){ ADC1->CR |= ADC_CR_ADSTP; while (ADC1->CR & ADC_CR_ADSTP){} }
  if (ADC1->CR & ADC_CR_ADEN){ ADC1->CR |= ADC_CR_ADDIS; while (ADC1->CR & ADC_CR_ADEN){} }

  /* CFGR1: limpa e reprograma */
  ADC1->CFGR1 = 0;
  ADC1->CFGR1 |= (uint32_t)cfg->resolution;
  if (cfg->align == ADC_BM_ALIGN_LEFT)   ADC1->CFGR1 |= ADC_CFGR1_ALIGN;
  if (cfg->scan_dir == ADC_BM_SCAN_DESC) ADC1->CFGR1 |= ADC_CFGR1_SCANDIR;
  if (cfg->overrun_overwrite)            ADC1->CFGR1 |= ADC_CFGR1_OVRMOD;

  /* SMPR */
  ADC1->SMPR = (uint32_t)cfg->sample_time;

  /* DMA bits no ADC (o controller é armado nas APIs) */
  if (cfg->dma_enable) {
    ADC1->CFGR1 |= ADC_CFGR1_DMAEN;
    if (cfg->dma_circular) ADC1->CFGR1 |= ADC_CFGR1_DMACFG;
    else                   ADC1->CFGR1 &= ~ADC_CFGR1_DMACFG;
  } else {
    ADC1->CFGR1 &= ~(ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG);
  }

  /* Gatilho externo */
  ADC1->CFGR1 &= ~(ADC_CFGR1_EXTSEL_Msk | ADC_CFGR1_EXTEN_Msk);
  ADC1->CFGR1 |= (uint32_t)cfg->extsel;
  ADC1->CFGR1 |= (uint32_t)cfg->extedge;
  s_extedge_cfg = cfg->extedge;

  /* Não esperamos ADRDY. ADEN + (ADSTART somente se extedge=DISABLED) será feito nas operações. */
}

/* ===== canais ===== */
void adc_bm_set_channels_mask(uint32_t chsel_mask){
  s_chsel_mask = chsel_mask & 0x07FFFFu;
  ADC1->CHSELR = s_chsel_mask;
  (void)ADC1->CHSELR;
  s_ch_count = popcount32(s_chsel_mask);
  s_seq_idx = 0;
}
void adc_bm_set_channels_list(const uint8_t *list, uint8_t n){
  uint32_t m=0;
  for (uint8_t i=0;i<n;i++){ if (list[i] <= 18u) m |= (1u << list[i]); }
  adc_bm_set_channels_mask(m);
}

/* ===== Polling ===== */
uint8_t adc_bm_read_sequence_polling(uint16_t *out, uint8_t max_samples){
  if (!out || max_samples==0 || s_ch_count==0) return 0;

  /* limpa flags e habilita ADC */
  ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR  |= ADC_CR_ADEN;

  /* dispara por software apenas se NÃO houver trigger externo */
  if (s_extedge_cfg == ADC_BM_EXT_DISABLED) {
    ADC1->CR  |= ADC_CR_ADSTART;
  }

  uint8_t n=0;
  while (n < max_samples){
    uint32_t to=0;
    while ((ADC1->ISR & ADC_ISR_EOC)==0){ if (++to > ADC_BM_TIMEOUT) return n; }
    out[n++] = read_dr_aligned();     /* ler DR limpa EOC */

    if (n >= s_ch_count){
      uint32_t t2=0; while ((ADC1->ISR & ADC_ISR_EOS)==0){ if (++t2 > ADC_BM_TIMEOUT) break; }
      ADC1->ISR = ADC_ISR_EOS;
      break;
    }
  }
  return n;
}

uint16_t adc_bm_read_single(uint8_t channel){
  uint32_t prev = s_chsel_mask; uint8_t prevc = s_ch_count;
  adc_bm_set_channels_mask((channel<=18u) ? (1u<<channel) : 0u);
  uint16_t v=0; (void)adc_bm_read_sequence_polling(&v,1);
  adc_bm_set_channels_mask(prev); s_ch_count = prevc;
  return v;
}

/* ===== IRQ do ADC ===== */
void adc_bm_it_start(uint8_t nvic_prio, bool eoc_irq, bool eos_irq, bool ovr_irq,
                     adc_bm_it_cb_t cb, void *ctx){
  s_it_cb = cb; s_it_ctx = ctx; s_seq_idx = 0;

  ADC1->IER = 0;
  if (eoc_irq) ADC1->IER |= ADC_IER_EOCIE;
  if (eos_irq) ADC1->IER |= ADC_IER_EOSIE;
  if (ovr_irq) ADC1->IER |= ADC_IER_OVRIE;

  /* limpa flags, habilita ADC e dispara se for software trigger */
  ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR  |= ADC_CR_ADEN;
  if (s_extedge_cfg == ADC_BM_EXT_DISABLED) {
    ADC1->CR  |= ADC_CR_ADSTART;
  }

  nvic_enable_irq(ADC1_COMP_IRQn, nvic_prio); /* seu enum mapeia ADC nesse nome */
}

void adc_bm_it_stop(void){
  ADC1->IER = 0;
}

/* Compatível com startup que chama ADC_IRQHandler */
void ADC_IRQHandler(void){
  uint32_t isr = ADC1->ISR;

  if (isr & ADC_ISR_OVR){ ADC1->ISR = ADC_ISR_OVR; }

  if (isr & ADC_ISR_EOC){
    uint16_t s = read_dr_aligned();
    uint8_t  i = s_seq_idx;
    if (s_it_cb) s_it_cb(s, i, false, s_it_ctx);
    if (++s_seq_idx >= s_ch_count) s_seq_idx = s_ch_count ? (s_ch_count-1) : 0;
  }

  if (isr & ADC_ISR_EOS){
    ADC1->ISR = ADC_ISR_EOS;
    s_seq_idx = 0;
    if (s_it_cb) s_it_cb(0, 0, true, s_it_ctx);
    /* re-dispara SW somente se não houver externo */
    if (s_extedge_cfg == ADC_BM_EXT_DISABLED) ADC1->CR |= ADC_CR_ADSTART;
  }
}

/* ===== DMA ===== */
static void adc_dma_router_cb(uint32_t flags, void *ctx){
  (void)ctx;
  if (s_dma_cb) s_dma_cb(flags, s_dma_ctx);
}

/* One-shot: N amostras, TC ao final */
bool adc_bm_dma_start_oneshot(uint16_t *dst, uint16_t count,
                              uint8_t dma_priority, bool te_irq,
                              adc_bm_dma_cb_t cb, void *ctx){
  if (!dst || count==0) return false;

  s_dma_cb = cb; s_dma_ctx = ctx;

  dma_router_init(/*prio*/1);
  dma_router_attach(ADC_DMA_CH, adc_dma_router_cb, NULL);
  dma_router_stop(ADC_DMA_CH);

  /* Habilita DMA no ADC e garante one-shot */
  ADC1->CFGR1 |= ADC_CFGR1_DMAEN;
  ADC1->CFGR1 &= ~ADC_CFGR1_DMACFG;

  dma_router_chan_cfg_t c = {
    .mem_to_periph=0, .circular=0, .minc=1, .pinc=0,
    .msize_bits=1, .psize_bits=1, .priority=(dma_priority&3),
    .irq_tc=1, .irq_ht=0, .irq_te= te_irq ? 1u : 0u
  };
  if (!dma_router_start(ADC_DMA_CH, (uint32_t)&ADC1->DR, (uint32_t)dst, count, &c))
    return false;

  /* limpa flags, habilita ADC; start SW só se não houver trigger externo */
  ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR  |= ADC_CR_ADEN;
  if (s_extedge_cfg == ADC_BM_EXT_DISABLED) {
    ADC1->CR  |= ADC_CR_ADSTART;
  }
  return true;
}

/* Circular (streaming) */
bool adc_bm_dma_start_circular(uint16_t *ring, uint16_t length,
                               uint8_t dma_priority, bool ht_irq, bool tc_irq, bool te_irq,
                               adc_bm_dma_cb_t cb, void *ctx){
  if (!ring || length==0) return false;

  s_dma_cb = cb; s_dma_ctx = ctx;

  dma_router_init(/*prio*/1);
  dma_router_attach(ADC_DMA_CH, adc_dma_router_cb, NULL);
  dma_router_stop(ADC_DMA_CH);

  ADC1->CFGR1 |= ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;

  dma_router_chan_cfg_t c = {
    .mem_to_periph=0, .circular=1, .minc=1, .pinc=0,
    .msize_bits=1, .psize_bits=1, .priority=(dma_priority&3),
    .irq_tc= tc_irq?1u:0u, .irq_ht= ht_irq?1u:0u, .irq_te= te_irq?1u:0u
  };
  if (!dma_router_start(ADC_DMA_CH, (uint32_t)&ADC1->DR, (uint32_t)ring, length, &c))
    return false;

  /* habilita ADC; start SW só se não houver trigger externo */
  ADC1->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
  ADC1->CR  |= ADC_CR_ADEN;
  if (s_extedge_cfg == ADC_BM_EXT_DISABLED) {
    ADC1->CR  |= ADC_CR_ADSTART;
  }
  return true;
}

void adc_bm_dma_stop(void){
  dma_router_stop(ADC_DMA_CH);
  dma_router_detach(ADC_DMA_CH);
  ADC1->CFGR1 &= ~(ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG);
}
