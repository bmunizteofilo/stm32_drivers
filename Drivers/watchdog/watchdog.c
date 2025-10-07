#include "watchdog.h"

/* Guard de faixa */
static inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi){
    return (v<lo)?lo: (v>hi)?hi: v;
}

/* helpers com timeout para nunca travar */
static inline int iwdg_wait_clear(uint32_t mask) {
    uint32_t to = 1000000u; /* ~guard arbitrary */
    while ((IWDG->SR & mask) != 0u) {
        if (--to == 0u) return 0; /* timeout -> não trava */
    }
    return 1;
}
/* ========================== IWDG ========================== */
void iwdg_bm_pick(uint32_t lsi_hz, uint32_t timeout_ms, uint8_t *pr_out, uint16_t *rlr_out)
{
    if (!lsi_hz) lsi_hz = 40000u;
    uint32_t best_err = 0xFFFFFFFFu;
    uint8_t  best_pr  = IWDG_PR_DIV4;
    uint16_t best_rlr = 1;

    const uint16_t divs[7] = {4,8,16,32,64,128,256};
    for (uint8_t pr=0; pr<7; ++pr){
        uint32_t p = divs[pr];
        /* RLR = timeout * lsi/p - 1 */
        uint32_t rlr = ((uint64_t)timeout_ms * lsi_hz) / (1000u * p);
        if (rlr==0) rlr = 1; /* mínimo prático */
        if (rlr > 4095u) rlr = 4095u;

        uint32_t achieved_ms = ( (rlr+1u) * p * 1000u ) / lsi_hz;
        uint32_t err = (achieved_ms > timeout_ms)? (achieved_ms - timeout_ms) : (timeout_ms - achieved_ms);

        if (err < best_err){ best_err = err; best_pr = pr; best_rlr = (uint16_t)rlr; }
        if (best_err == 0) break;
    }
    if (pr_out)  *pr_out  = best_pr;
    if (rlr_out) *rlr_out = best_rlr;
}


void iwdg_bm_init_start(const iwdg_bm_cfg_t *cfg)
{
    uint8_t  pr  = (uint8_t)(cfg->pr & 0x7);
    uint16_t rlr = (cfg->rlr == 0) ? 1 : (cfg->rlr > 4095 ? 4095 : cfg->rlr);
    uint16_t win = (cfg->win > 4095) ? 4095 : cfg->win;

    /* 1) (Seguro) garanta LSI ON — evita domínio parado */
    RCC->CSR |= RCC_CSR_LSION;
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0) { /* spin */ }

    /* 2) desbloqueia */
    IWDG->KR = IWDG_KR_UNLOCK;

    /* 3) espere estar livre ANTES de cada write */
    (void)iwdg_wait_clear(IWDG_SR_PVU);
    IWDG->PR = pr;

    (void)iwdg_wait_clear(IWDG_SR_RVU);
    IWDG->RLR = rlr;

    /* F070 não tem WVU; WINR pode ser escrito direto */
    IWDG->WINR = win;  /* use 0x0FFF para “sem janela” */

    /* 4) start + primeiro reload */
    IWDG->KR = IWDG_KR_START;
    IWDG->KR = IWDG_KR_RELOAD;
}

/* ========================== WWDG ========================== */
static wwdg_bm_cb_t s_ewi_cb  = 0;
static void        *s_ewi_ctx = 0;

void wwdg_bm_init(const wwdg_bm_cfg_t *cfg, uint8_t t_init)
{
    /* <- LIGA O CLOCK DO WWDG */
    RCC->APB1ENR |= RCC_APB1ENR_WWDGEN;

    uint32_t cfr = 0;
    cfr |= (uint32_t)(cfg->window & WWDG_CFR_W_Msk);
    cfr |= ((uint32_t)(cfg->wdgtb & 3u) << WWDG_CFR_WDGTB_Pos);
    if (cfg->ewi_enable) cfr |= WWDG_CFR_EWI;

    WWDG->CFR = cfr;
    if (cfg->ewi_enable){
        WWDG->SR = 0; /* limpa EWIF */
        s_ewi_cb  = cfg->cb;
        s_ewi_ctx = cfg->cb_ctx;
        nvic_enable_irq(WWDG_IRQn, cfg->nvic_prio);
    }

    uint8_t t = (t_init & 0x7F);
    if (t < 0x40) t = 0x7F;
    WWDG->CR = (uint32_t)WWDG_CR_WDGA | t;   /* start + carga */

    /* opcional: sanity — ver se o contador começou a descer */
    //for (volatile uint32_t i=0;i<20000;i++) { __asm volatile ("nop"); }
    /* agora wwdg_bm_counter_now() deve ser < 0x7F depois de alguns us */
}

/* ===== WWDG: cálculo de tempos ===== */
static inline uint32_t wwdg_tick_hz(uint32_t pclk_hz, uint8_t wdgtb) {
    /* f_tick = PCLK / 4096 / (2^wdgtb) */
    return (pclk_hz / 4096u) >> (wdgtb & 3u);
}
uint32_t wwdg_bm_time_to_ewi_ms(uint32_t pclk_hz, uint8_t wdgtb, uint8_t t_reload) {
    if (t_reload < 0x41) t_reload = 0x41;   /* precisa ser > 0x40 */
    if (t_reload > 0x7F) t_reload = 0x7F;
    uint32_t steps = (uint32_t)t_reload - 0x40u; /* #decrements até T==0x40 */
    uint32_t f = wwdg_tick_hz(pclk_hz, wdgtb);
    return (steps * 1000u + (f/2)) / f;     /* arredondamento */
}
uint32_t wwdg_bm_time_window_open_ms(uint32_t pclk_hz, uint8_t wdgtb, uint8_t t_reload, uint8_t window) {
    if (t_reload < 0x41) t_reload = 0x41;
    if (t_reload > 0x7F) t_reload = 0x7F;
    if (window   < 0x40) window   = 0x40;
    if (window   > 0x7F) window   = 0x7F;
    /* janela abre quando T < window; tempo até isso é (t_reload - window) ticks */
    uint32_t steps = (t_reload > window) ? ((uint32_t)t_reload - (uint32_t)window) : 0u;
    uint32_t f = wwdg_tick_hz(pclk_hz, wdgtb);
    return (steps * 1000u + (f/2)) / f;
}

/* ===== WWDG: picker por tempo alvo de EWI ===== */
uint32_t wwdg_bm_pick_by_ewi(uint32_t pclk_hz, uint32_t ewi_target_ms,
                             uint8_t window_open_pct,
                             uint8_t *wdgtb_out, uint8_t *t_reload_out, uint8_t *window_out)
{
    if (window_open_pct > 100u) window_open_pct = 100u;
    uint32_t best_err = 0xFFFFFFFFu;
    uint8_t  best_tb = 0, best_tr = 0x7F;

    for (uint8_t tb=0; tb<4; ++tb) {
        uint32_t f = wwdg_tick_hz(pclk_hz, tb);
        if (f == 0u) continue;

        /* steps desejados até EWI: steps ≈ ewi_ms * f / 1000, limitado a [1..63] */
        uint32_t steps = (ewi_target_ms * f + 500u) / 1000u; /* arredonda */
        if (steps < 1u)  steps = 1u;   /* pelo menos 1 tick */
        if (steps > 63u) steps = 63u;  /* t_reload no máx 0x7F (127): 127-64=63 */

        uint8_t t_reload = (uint8_t)(0x40u + steps);
        if (t_reload < 0x41) t_reload = 0x41;
        if (t_reload > 0x7F) t_reload = 0x7F;

        uint32_t got_ms = wwdg_bm_time_to_ewi_ms(pclk_hz, tb, t_reload);
        uint32_t err = (got_ms > ewi_target_ms) ? (got_ms - ewi_target_ms) : (ewi_target_ms - got_ms);

        if (err < best_err) { best_err = err; best_tb = tb; best_tr = t_reload; }
        if (best_err == 0u) break;
    }

    /* Janela: abre em “window_open_pct” do caminho até o EWI.
       window = 0x40 + floor(pct * (t_reload-0x40)) */
    uint32_t span = (uint32_t)best_tr - 0x40u;
    uint32_t w_steps = (span * (uint32_t)window_open_pct + 50u) / 100u; /* arredonda */
    uint8_t  window  = (uint8_t)(0x40u + ((w_steps > span)? span : w_steps));
    if (window < 0x40) window = 0x40;
    if (window > 0x7F) window = 0x7F;

    if (wdgtb_out)    *wdgtb_out    = best_tb;
    if (t_reload_out) *t_reload_out = best_tr;
    if (window_out)   *window_out   = window;
    return best_err;
}

/* IRQ de Early Wakeup (quando T chega a 0x40) */
void WWDG_IRQHandler(void)
{
    WWDG->SR = 0; /* limpa EWIF */
    if (s_ewi_cb) s_ewi_cb(s_ewi_ctx);
    /* Dica: se quiser evitar reset só via EWI, pode dar “kick” aqui:
       wwdg_bm_kick(0x7F);
       Mas normalmente você só sinaliza e deixa a aplicação decidir. */
}
