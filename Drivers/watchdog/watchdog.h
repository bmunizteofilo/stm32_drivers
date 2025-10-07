#ifndef WDG_BM_H
#define WDG_BM_H

#include "stm32f070xx.h"

/* ========================== IWDG ========================== */
typedef struct {
    uint32_t lsi_hz;     /* Hz do LSI (típico ~40000). Se zero, assume 40000. */
    uint8_t  pr;         /* IWDG_PR_DIV(4..256) */
    uint16_t rlr;        /* 0..4095 (tempo = (rlr+1)*prescaler/LSI) */
    uint16_t win;        /* 0..4095; 0x0FFF “desabilita” janela (janela ampla). */
} iwdg_bm_cfg_t;

/* Helpers de cálculo (opcionais) */
void iwdg_bm_pick(uint32_t lsi_hz, uint32_t timeout_ms, uint8_t *pr_out, uint16_t *rlr_out);

/* Programa PR/RLR/WINR e dá START (irrevogável até reset) */
void iwdg_bm_init_start(const iwdg_bm_cfg_t *cfg);

/* “Kick” (reload). Pode ser chamado de qualquer lugar. */
static inline void iwdg_bm_kick(void){ IWDG->KR = IWDG_KR_RELOAD; }

/* ========================== WWDG ========================== */
typedef void (*wwdg_bm_cb_t)(void *ctx);  /* chamado no EWI (counter chegou a 0x40) */

typedef struct {
    /* clock: f_wwdg = PCLK / 4096 / 2^wdgtb  (wdgtb ∈ {0,1,2,3}) */
    uint8_t  wdgtb;        /* 0:/1, 1:/2, 2:/4, 3:/8 */
    uint8_t  window;       /* W[6:0]; 0x40..0x7F usual. Refresh deve ocorrer com T < W. */
    bool     ewi_enable;   /* liga Early Wakeup Interrupt */
    uint8_t  nvic_prio;    /* prioridade NVIC para WWDG_IRQHandler */
    wwdg_bm_cb_t cb;       /* callback EWI (opcional) */
    void    *cb_ctx;
} wwdg_bm_cfg_t;

/* Inicializa WWDG (CFR, EWI/NVIC) e opcionalmente “starta” com T inicial. */
void wwdg_bm_init(const wwdg_bm_cfg_t *cfg, uint8_t t_init /*ex.: 0x7F*/);

/* “Kick” do WWDG: reescreve T (e mantém WDGA=1).
   ATENÇÃO: só chame dentro da janela (T < W), senão ocorre reset imediato. */
static inline void wwdg_bm_kick(uint8_t t_reload){
    WWDG->CR = (uint32_t)WWDG_CR_WDGA | (t_reload & WWDG_CR_T_Msk);
}

/* Permite ler o T atual (contador descendo). */
static inline uint8_t wwdg_bm_counter_now(void){
    return (uint8_t)(WWDG->CR & WWDG_CR_T_Msk);
}

/* ISR a ser chamada pelo vetor (seu startup já aponta para WWDG_IRQHandler). */
void WWDG_IRQHandler(void);

/* === (NOVO) helpers WWDG: escolha automática ===
   Dado PCLK (Hz) e alvo de tempo até EWI (ms), escolhe wdgtb (0..3) e t_reload (0x41..0x7F).
   window_open_pct (0..100) define quando a janela abre (ex.: 75 → abre aos 75% do caminho até o EWI).
   Retorna o erro (|alvo - obtido|) em milissegundos. */
uint32_t wwdg_bm_pick_by_ewi(uint32_t pclk_hz, uint32_t ewi_target_ms,
                             uint8_t window_open_pct /*0..100*/,
                             uint8_t *wdgtb_out, uint8_t *t_reload_out, uint8_t *window_out);

/* Quanto tempo (ms) até o EWI acontecer (T==0x40), dado wdgtb/t_reload e PCLK. */
uint32_t wwdg_bm_time_to_ewi_ms(uint32_t pclk_hz, uint8_t wdgtb, uint8_t t_reload);

/* Quando (ms) a janela abre (T < window), contado desde o start com t_reload. */
uint32_t wwdg_bm_time_window_open_ms(uint32_t pclk_hz, uint8_t wdgtb, uint8_t t_reload, uint8_t window);

/* === (NOVO) freeze em debug (IWDG/WWDG) === */
static inline void wdg_bm_debug_freeze_enable(bool iwdg, bool wwdg) {
    uint32_t m = 0;
    if (iwdg) m |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    if (wwdg) m |= DBGMCU_APB1_FZ_DBG_WWDG_STOP;
    DBGMCU->APB1FZ |= m;
}
static inline void wdg_bm_debug_freeze_disable(bool iwdg, bool wwdg) {
    uint32_t m = 0;
    if (iwdg) m |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    if (wwdg) m |= DBGMCU_APB1_FZ_DBG_WWDG_STOP;
    DBGMCU->APB1FZ &= ~m;
}

#endif /* WDG_BM_H */
