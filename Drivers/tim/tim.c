#include "tim.h"


/*====================== Handles globais p/ ISRs ===================== */
static tim_handle_t *g_tim1=0, *g_tim3=0, *g_tim14=0, *g_tim16=0, *g_tim17=0;

uint16_t tim_read_ccr(TIM_TypeDef *t, uint8_t ch){
  switch (ch){
    case 1: return (uint16_t)t->CCR1;
    case 2: return (uint16_t)t->CCR2;
    case 3: return (uint16_t)t->CCR3;
    default:return (uint16_t)t->CCR4;
  }
}

/* ===================== Clock de periférico ===================== */
void tim_enable_clock(TIM_TypeDef *t){
  if (t==TIM1)      RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  else if (t==TIM16)RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
  else if (t==TIM17)RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
  else if (t==TIM3) RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  else if (t==TIM14)RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
}

/* ===================== Timebase ===================== */
static void tim_apply_timebase(TIM_TypeDef *t, const tim_init_t *cfg){
  /* para contador */
  t->CR1 &= ~(1u<<0);

  /* direção/center e opções */
  uint32_t cr1 = t->CR1 & ~((1u<<4)|(3u<<5)|(1u<<7)|(1u<<3));
  switch (cfg->mode){
    case TIM_COUNT_UP:      break;
    case TIM_COUNT_DOWN:    cr1 |= (1u<<4); break;      /* DIR */
    case TIM_COUNT_CENTER1: cr1 |= (1u<<5); break;      /* CMS=01 */
    case TIM_COUNT_CENTER2: cr1 |= (2u<<5); break;      /* CMS=10 */
    case TIM_COUNT_CENTER3: cr1 |= (3u<<5); break;      /* CMS=11 */
  }
  if (cfg->arpe)      cr1 |= (1u<<7);
  if (cfg->one_pulse) cr1 |= (1u<<3);
  t->CR1 = cr1;

  /* PSC/ARR: por freq ou direto */
  if (cfg->freq_hz){
    uint32_t psc = (cfg->clk_hz + (cfg->freq_hz*65536u - 1)) / (cfg->freq_hz*65536u);
    if (psc > 0xFFFFu) psc = 0xFFFF;
    uint32_t arr = (cfg->clk_hz / (psc+1u)) / cfg->freq_hz;
    if (arr==0) arr = 1;
    arr -= 1u;
    if (arr > 0xFFFFu) arr = 0xFFFF;
    t->PSC = (uint16_t)psc;
    t->ARR = (uint16_t)arr;
  } else {
    t->PSC = cfg->psc;
    t->ARR = cfg->arr;
  }

  /* RCR (só TIM1) */
  if (t==TIM1) t->RCR = cfg->repetition;

  /* TRGO (CR2.MMS) */
  tim_set_mms(t, cfg->mms & 7u);

  /* UG para carregar buffers */
  t->EGR = 1u; /* UG */
}

void tim_init(tim_handle_t *h, TIM_TypeDef *t, const tim_init_t *cfg){
  memset(h, 0, sizeof(*h));
  h->tim = t; h->is_tim1 = (t==TIM1);

  tim_enable_clock(t);
  tim_apply_timebase(t, cfg);

  /* IRQs no NVIC — deixamos prontos; o usuário habilita DIER via API de callback */
  if (t==TIM1){
    g_tim1 = h;
    nvic_enable_irq(TIM1_BRK_UP_TRG_COM_IRQn, cfg->nvic_prio);
    nvic_enable_irq(TIM1_CC_IRQn, cfg->nvic_prio);
  } else if (t==TIM3){
    g_tim3 = h;
    nvic_enable_irq(TIM3_IRQn, cfg->nvic_prio);
  } else if (t==TIM14){
    g_tim14 = h;
    nvic_enable_irq(TIM14_IRQn, cfg->nvic_prio);
  } else if (t==TIM16){
    g_tim16 = h;
    nvic_enable_irq(TIM16_IRQn, cfg->nvic_prio);
  } else if (t==TIM17){
    g_tim17 = h;
    nvic_enable_irq(TIM17_IRQn, cfg->nvic_prio);
  }
}

void tim_start(TIM_TypeDef *t){ t->CR1 |= (1u<<0); }
void tim_stop (TIM_TypeDef *t){ t->CR1 &= ~(1u<<0); }

void tim_set_freq(TIM_TypeDef *t, uint32_t clk_hz, uint32_t freq_hz){
  uint32_t psc = (clk_hz + (freq_hz*65536u - 1)) / (freq_hz*65536u);
  if (psc > 0xFFFFu) psc = 0xFFFF;
  uint32_t arr = (clk_hz / (psc+1u)) / freq_hz;
  if (arr==0) arr=1;
  arr -= 1u;
  if (arr > 0xFFFFu) arr=0xFFFF;
  t->PSC = (uint16_t)psc;
  t->ARR = (uint16_t)arr;
  t->EGR = 1u;
}

/* ===================== PWM / OC ===================== */
static volatile uint32_t* tim_ccr_ptr(TIM_TypeDef *t, uint8_t ch) {
	switch (ch) {
	case 1:
		return &t->CCR1;
	case 2:
		return &t->CCR2;
	case 3:
		return &t->CCR3;
	default:
		return &t->CCR4;
	}
}

/* Configura modo de OC/PWM no canal (preload opcional) */
void tim_set_oc_mode(TIM_TypeDef *t, uint8_t ch, tim_oc_mode_t mode, uint8_t preload){
  uint32_t *ccmr = (ch<=2) ? &t->CCMR1 : &t->CCMR2;
  uint8_t off = (ch==1||ch==3) ? 0 : 8;
  uint32_t v = *ccmr;
  /* limpa OCxM/OCxPE/CCxS */
  v &= ~( (3u<<off) | (1u<<(off+3)) | (7u<<(off+4)) );
  v |= ((uint32_t)mode & 7u) << (off+4);    /* OCxM */
  if (preload) v |= (1u << (off+3));        /* OCxPE */
  *ccmr = v;
}
/* Ajusta CCR */
void tim_pwm_set_compare(TIM_TypeDef *t, uint8_t ch, uint16_t ccr){
  *tim_ccr_ptr(t,ch) = ccr;
}
/* Polaridade (1=ativo em alto, 0=ativo em baixo => CCxP=0/1 invertido) */
void tim_pwm_polarity(TIM_TypeDef *t, uint8_t ch, uint8_t active_high){
  uint32_t bit = (1u << (((ch-1)<<2)+1)); /* CCxP */
  if (active_high) t->CCER &= ~bit; else t->CCER |= bit;
}
/* Enable da saída (CCxE) */
void tim_pwm_enable(TIM_TypeDef *t, uint8_t ch, uint8_t enable){
  uint32_t bit = (1u << (((ch-1)<<2)+0)); /* CCxE */
  if (enable) t->CCER |= bit; else t->CCER &= ~bit;
}

/* ===================== Avançado (TIM1) ===================== */
void tim1_enable_complementary(uint8_t ch, uint8_t en){
  uint32_t bit = (1u << (((ch-1)<<2)+2)); /* CCxNE */
  if (en) TIM1->CCER |= bit; else TIM1->CCER &= ~bit;
}
void tim1_set_deadtime(uint8_t dt_ticks){
  TIM1->BDTR = (TIM1->BDTR & ~0xFFu) | (dt_ticks & 0xFFu);
}
void tim1_main_output_enable(uint8_t en){
  if (en) TIM1->BDTR |= (1u<<15); else TIM1->BDTR &= ~(1u<<15); /* MOE */
}

/* ===================== Input Capture ===================== */
void tim_ic_config(TIM_TypeDef *t, uint8_t ch, tim_ic_edge_t edge, uint8_t filter){
  uint32_t *ccmr = (ch<=2) ? &t->CCMR1 : &t->CCMR2;
  uint8_t off = (ch==1||ch==3) ? 0 : 8;
  uint32_t v = *ccmr;
  v &= ~(0xFFu << off);
  v |= (1u << off);                                 /* CCxS=01 => TIx */
  v |= ((uint32_t)(filter & 0xF) << (off+4));       /* ICxF */
  *ccmr = v;

  /* Polarity e enable */
  uint32_t ccer = t->CCER;
  uint8_t sh = ((ch-1)<<2);
  ccer &= ~(0xFu << sh);
  if (edge == TIM_EDGE_RISING)       ccer |= (1u<<sh);                     /* CCxE=1, CCxP=0 */
  else if (edge == TIM_EDGE_FALLING) ccer |= (1u<<sh) | (1u<<(sh+1));      /* CCxE=1, CCxP=1 */
  else { /* BOTH */
    /* Nos GP do F0 não há CCxNP; usamos “falling” como aproximação. */
    ccer |= (1u<<sh) | (1u<<(sh+1));
  }
  t->CCER = ccer;
}

/* ===================== Callbacks/IRQs ===================== */
void tim_on_update(tim_handle_t *h, tim_cb_t cb, void *ctx){
  h->on_update = cb; h->on_update_ctx = ctx;
  if (cb) h->tim->DIER |=  (1u<<0); else h->tim->DIER &= ~(1u<<0); /* UIE */
}
void tim_on_cc(tim_handle_t *h, uint8_t ch, tim_cb_t cb, void *ctx){
  if (ch<1 || ch>4) return;
  h->on_cc[ch-1] = cb; h->on_cc_ctx[ch-1] = ctx;
  uint32_t bit = (1u<<ch); /* CCxIE em DIER */
  if (cb) h->tim->DIER |= bit; else h->tim->DIER &= ~bit;
}

static void tim_dispatch(TIM_TypeDef *t, tim_handle_t *h){
  if (!h) return;
  uint32_t sr = t->SR;

  /* Update */
  if ((sr & 1u) && (t->DIER & 1u)){
    t->SR &= ~1u;
    if (h->on_update) h->on_update(sr, h->on_update_ctx);
  }
  /* CC1..CC4 */
  for (uint8_t i=0;i<4;i++){
    uint32_t mask = (1u<<(i+1));
    if ((sr & mask) && (t->DIER & mask)){
      t->SR &= ~mask;
      if (h->on_cc[i]) h->on_cc[i](sr, h->on_cc_ctx[i]);
    }
  }
}

/* ISRs mínimas (adicione no seu vetor de interrupções do startup) */
void TIM1_BRK_UP_TRG_COM_IRQHandler(void) {
	tim_dispatch(TIM1, g_tim1);
}
void TIM1_CC_IRQHandler(void) {
	tim_dispatch(TIM1, g_tim1);
}
void TIM3_IRQHandler(void) {
	tim_dispatch(TIM3, g_tim3);
}
void TIM14_IRQHandler(void) {
	tim_dispatch(TIM14, g_tim14);
}
void TIM16_IRQHandler(void) {
	tim_dispatch(TIM16, g_tim16);
}
void TIM17_IRQHandler(void) {
	tim_dispatch(TIM17, g_tim17);
}

/* ===================== DMA requests (DIER) ===================== */
void tim_dma_enable_update(TIM_TypeDef *t, uint8_t en){
  if (en) t->DIER |=  (1u<<8); else t->DIER &= ~(1u<<8);  /* UDE */
}
void tim_dma_enable_cc(TIM_TypeDef *t, uint8_t ch, uint8_t en){
  uint32_t bit = (1u << (ch+8)); /* CC1DE=9, CC2DE=10, ... */
  if (en) t->DIER |= bit; else t->DIER &= ~bit;
}
