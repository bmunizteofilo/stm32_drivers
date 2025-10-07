#include "systick.h"

static volatile uint64_t s_ticks64 = 0;
static void (*s_cb)(void) = 0;
static systick_info_t s_info = {0};

/* ------- Prioridade “raw” (Cortex-M0, 0..3; usa 2 MSBs) ------- */
static inline void systick_set_priority_raw(int priority_0_to_3) {
    if (priority_0_to_3 < 0) return;
    if (priority_0_to_3 > 3) priority_0_to_3 = 3;
    uint32_t shpr3 = SCB_SHPR3;
    uint32_t pri   = (uint32_t)(priority_0_to_3 & 0x3u) << 6; /* 0,64,128,192 */
    shpr3 &= ~(0xFFu << 24);      /* limpa byte do SysTick (PRI_15) */
    shpr3 |=  (pri   << 24);      /* seta novo valor */
    SCB_SHPR3 = shpr3;
}

/* ------- Programa RELOAD e fonte ------- */
static bool systick_program_reload(uint32_t hclk_hz, uint32_t tick_hz, bool use_ahb) {
    if (tick_hz == 0) return false;

    uint32_t src = use_ahb ? hclk_hz : (hclk_hz / 8u);
    if (!src) return false;

    /* RELOAD = src/tick_hz - 1  (1..0xFFFFFF) */
    uint32_t reload = (src + tick_hz/2u) / tick_hz; /* arredonda */
    if (reload == 0) reload = 1;
    if (reload > 0x01000000UL) return false; /* 24 bits + 1 */
    reload -= 1u;
    if (reload > 0x00FFFFFFUL) return false;

    /* Programa com contador parado para evitar “salto” */
    uint32_t csr = SYST_CSR & ~(SYST_CSR_ENABLE);
    SYST_CSR = csr;            /* pausa, preserva TICKINT/CLKSOURCE */
    SYST_RVR = reload;
    SYST_CVR = 0;              /* zera o VAL para começar ciclo cheio */

    /* Fonte */
    if (use_ahb) SYST_CSR = (SYST_CSR | SYST_CSR_CLKSOURCE);
    else         SYST_CSR = (SYST_CSR & ~SYST_CSR_CLKSOURCE);

    s_info.hclk_hz  = hclk_hz;
    s_info.tick_hz  = tick_hz;
    s_info.reload   = reload;
    s_info.use_ahb  = use_ahb ? 1u : 0u;
    return true;
}

/* ------- Aplica IRQ e prioridade ------- */
static inline void systick_apply_irq(bool irq_enable, int priority_0_to_3) {
    /* prioridade do SysTick por registrador (sem CMSIS) */
    if (priority_0_to_3 >= 0) systick_set_priority_raw(priority_0_to_3);
    if (irq_enable) { SYST_CSR |=  SYST_CSR_TICKINT; s_info.irq_enabled = 1; }
    else            { SYST_CSR &= ~SYST_CSR_TICKINT; s_info.irq_enabled = 0; }
}

/* ------- API ------- */
bool systick_init_hz(uint32_t hclk_hz, uint32_t tick_hz,
                     bool use_ahb, bool irq_enable, int priority_0_to_3)
{
    if (!systick_program_reload(hclk_hz, tick_hz, use_ahb)) return false;
    s_ticks64 = 0;
    systick_apply_irq(irq_enable, priority_0_to_3);
    SYST_CSR |= SYST_CSR_ENABLE;
    return true;
}

bool systick_init_ms(uint32_t hclk_hz, uint32_t period_ms,
                     bool use_ahb, bool irq_enable, int priority_0_to_3)
{
    if (!period_ms) return false;
    /* ideal: tick_hz = 1000/period_ms, arredondado */
    uint32_t hz = (1000u + (period_ms/2u)) / period_ms;
    return systick_init_hz(hclk_hz, hz, use_ahb, irq_enable, priority_0_to_3);
}

bool systick_init_us(uint32_t hclk_hz, uint32_t period_us,
                     bool use_ahb, bool irq_enable, int priority_0_to_3)
{
    if (!period_us) return false;
    uint32_t hz = (1000000u + (period_us/2u)) / period_us;
    return systick_init_hz(hclk_hz, hz, use_ahb, irq_enable, priority_0_to_3);
}

/* Reconfiguração mantendo fonte atual */
bool systick_update_hz(uint32_t hclk_hz, uint32_t tick_hz) {
    return systick_program_reload(hclk_hz, tick_hz, s_info.use_ahb != 0);
}
bool systick_update_ms(uint32_t hclk_hz, uint32_t period_ms) {
    if (!period_ms) return false;
    uint32_t hz = (1000u + (period_ms/2u)) / period_ms;
    return systick_update_hz(hclk_hz, hz);
}
bool systick_update_us(uint32_t hclk_hz, uint32_t period_us) {
    if (!period_us) return false;
    uint32_t hz = (1000000u + (period_us/2u)) / period_us;
    return systick_update_hz(hclk_hz, hz);
}

void systick_set_callback(void (*cb)(void)) {
    s_cb = cb;
}

void systick_get_info(systick_info_t *out) {
    if (!out) return;
    *out = s_info;
}

uint64_t systick_ticks64(void) {
    return s_ticks64;
}

/* ------- Delays ------- */
void systick_delay_ms(uint32_t hclk_hz, uint32_t ms) {
    /* Se tick == 1ms com IRQ ligado, use ticks (preciso e barato) */
    if (s_info.tick_hz == 1000u && s_info.irq_enabled) {
        uint64_t target = s_ticks64 + ms;
        while ((int64_t)(s_ticks64 - target) < 0) { __asm volatile("nop"); }
        return;
    }
    /* Fallback: busy loop por ciclos (aproximado) */
    uint64_t cycles = ((uint64_t)hclk_hz * ms) / 1000ull;
    while (cycles--) { __asm volatile("nop"); }
}

void systick_delay_us(uint32_t hclk_hz, uint32_t us) {
    uint64_t cycles = ((uint64_t)hclk_hz * us) / 1000000ull;
    while (cycles--) { __asm volatile("nop"); }
}

/* ------- ISR ------- */
void systick_isr(void) {
    (void)SYST_CSR; /* lê COUNTFLAG (bit16) para limpar evento */
    s_ticks64++;
    if (s_cb) s_cb();
}

/* Handler opcional (se definido no build) */
void SysTick_Handler(void) {
    systick_isr();
}

