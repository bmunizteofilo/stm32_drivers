#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#include "stm32f070xx.h"

/* ===== Tipos ===== */
typedef struct {
    uint32_t hclk_hz;     /* clock do core */
    uint32_t tick_hz;     /* frequência programada */
    uint32_t reload;      /* valor em RVR (0..0xFFFFFF) */
    uint8_t  use_ahb;     /* 1=AHB, 0=AHB/8 */
    uint8_t  irq_enabled; /* 1 se TICKINT ligado */
} systick_info_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ===== API ===== */

/* Inicializações */
bool systick_init_hz(uint32_t hclk_hz, uint32_t tick_hz,
                     bool use_ahb, bool irq_enable, int priority_0_to_3);
bool systick_init_ms(uint32_t hclk_hz, uint32_t period_ms,
                     bool use_ahb, bool irq_enable, int priority_0_to_3);
bool systick_init_us(uint32_t hclk_hz, uint32_t period_us,
                     bool use_ahb, bool irq_enable, int priority_0_to_3);

/* Liga/desliga */
static inline void systick_enable(void)     { SYST_CSR |=  SYST_CSR_ENABLE; }
static inline void systick_disable(void)    { SYST_CSR &= ~SYST_CSR_ENABLE; }
static inline void systick_irq_enable(void) { SYST_CSR |=  SYST_CSR_TICKINT; }
static inline void systick_irq_disable(void){ SYST_CSR &= ~SYST_CSR_TICKINT; }

/* Reconfigurar período */
bool systick_update_hz(uint32_t hclk_hz, uint32_t tick_hz);
bool systick_update_ms(uint32_t hclk_hz, uint32_t period_ms);
bool systick_update_us(uint32_t hclk_hz, uint32_t period_us);

/* Callback por tick (opcional) */
void systick_set_callback(void (*cb)(void));

/* Info atual */
void systick_get_info(systick_info_t *out);

/* Contador de ticks (incrementa no ISR) */
uint64_t systick_ticks64(void);

/* Delays bloqueantes */
void systick_delay_ms(uint32_t hclk_hz, uint32_t ms);
void systick_delay_us(uint32_t hclk_hz, uint32_t us);

/* ISR do driver (chame a partir do seu SysTick_Handler) */
void systick_isr(void);

/* Opcional: forneça o handler pronto definindo SYSTICK_DRIVER_PROVIDES_HANDLER */
#ifdef SYSTICK_DRIVER_PROVIDES_HANDLER
void SysTick_Handler(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYSTICK_H__ */
