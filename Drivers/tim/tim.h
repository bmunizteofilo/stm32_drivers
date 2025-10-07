
#ifndef __TIM_H__
#define __TIM_H__

#include "stm32f070xx.h"

/* ===================== CONFIG/ENUMS ===================== */
typedef enum {
	TIM_COUNT_UP = 0,
	TIM_COUNT_DOWN = 1,
	TIM_COUNT_CENTER1 = 2,
	TIM_COUNT_CENTER2 = 3,
	TIM_COUNT_CENTER3 = 4
} tim_count_mode_t;
typedef enum {
	TIM_OCM_FROZEN = 0,
	TIM_OCM_ACTIVE = 1,
	TIM_OCM_INACTIVE = 2,
	TIM_OCM_TOGGLE = 3,
	TIM_OCM_FORCE_LOW = 4,
	TIM_OCM_FORCE_HIGH = 5,
	TIM_OCM_PWM1 = 6,
	TIM_OCM_PWM2 = 7
} tim_oc_mode_t;
typedef enum {
	TIM_EDGE_RISING = 0,
	TIM_EDGE_FALLING = 1,
	TIM_EDGE_BOTH = 2
} tim_ic_edge_t;

/* Callback (passamos SR bruto) */
typedef void (*tim_cb_t)(uint32_t sr, void *ctx);

typedef struct {
  /* timebase */
  uint32_t      clk_hz;        /* clock de entrada do timer */
  uint32_t      freq_hz;       /* se !=0, calcula PSC/ARR automaticamente */
  uint16_t      psc;           /* usado se freq_hz==0 */
  uint16_t      arr;           /* usado se freq_hz==0 */
  tim_count_mode_t mode;       /* up/down/center* */
  uint8_t       arpe;          /* auto-reload preload */
  uint8_t       one_pulse;     /* OPM */
  uint8_t       repetition;    /* só TIM1 (RCR) */

  /* master mode (TRGO) → CR2.MMS */
  uint8_t       mms;           /* 0..7 (ex.: 2=Update) */

  /* IRQ prioridade (0..3 efetivo no M0) */
  uint8_t       nvic_prio;
} tim_init_t;

typedef struct {
  TIM_TypeDef *tim;
  /* Callbacks */
  tim_cb_t  on_update;   void *on_update_ctx;
  tim_cb_t  on_cc[4];    void *on_cc_ctx[4];
  /* Interno */
  uint8_t   is_tim1;
} tim_handle_t;



/* ===================== API ===================== */


void tim_enable_clock(TIM_TypeDef *t);
void tim_init(tim_handle_t *h, TIM_TypeDef *t, const tim_init_t *cfg);
void tim_start(TIM_TypeDef *t);
void tim_stop(TIM_TypeDef *t);
void tim_set_freq(TIM_TypeDef *t, uint32_t clk_hz, uint32_t freq_hz);

/* Output Compare / PWM */
void tim_set_oc_mode(TIM_TypeDef *t, uint8_t ch, tim_oc_mode_t mode, uint8_t preload);
void tim_pwm_set_compare(TIM_TypeDef *t, uint8_t ch, uint16_t ccr);
void tim_pwm_polarity(TIM_TypeDef *t, uint8_t ch, uint8_t active_high);
void tim_pwm_enable(TIM_TypeDef *t, uint8_t ch, uint8_t enable);

/* Complementar / Deadtime / MOE (TIM1) */
void tim1_enable_complementary(uint8_t ch, uint8_t enable);
void tim1_set_deadtime(uint8_t dt_ticks);
void tim1_main_output_enable(uint8_t enable);

/* Input Capture */
void tim_ic_config(TIM_TypeDef *t, uint8_t ch, tim_ic_edge_t edge, uint8_t filter);

/* Callbacks/IRQs */
void tim_on_update(tim_handle_t *h, tim_cb_t cb, void *ctx);
void tim_on_cc(tim_handle_t *h, uint8_t ch, tim_cb_t cb, void *ctx);

/* DMA request enable/disable (só liga o pedido no DIER) */
void tim_dma_enable_update(TIM_TypeDef *t, uint8_t enable);
void tim_dma_enable_cc(TIM_TypeDef *t, uint8_t ch, uint8_t enable);

/* TRGO helper: CR2.MMS = 0..7 (ex.: 2=Update) */
static inline void tim_set_mms(TIM_TypeDef *t, uint8_t mms){
  t->CR2 = (t->CR2 & ~(7u<<4)) | ((uint32_t)(mms & 7u) << 4);
}

/* Ler CCRx (útil p/ IC e OC “incremental”) */
uint16_t tim_read_ccr(TIM_TypeDef *t, uint8_t ch);

/* Alias semântico p/ OC: habilita/desabilita a saída do canal (CCxE) */
static inline void tim_oc_output_enable(TIM_TypeDef *t, uint8_t ch, uint8_t en){
    tim_pwm_enable(t, ch, en);
}

/* Alias semântico p/ OC: escreve CCRx */
static inline void tim_set_compare(TIM_TypeDef *t, uint8_t ch, uint16_t val){
    tim_pwm_set_compare(t, ch, val);
}
#endif
