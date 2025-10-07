#include "rcc.h"

/* Limites práticos do F070 */
#define HSI_HZ              8000000UL
#define HSI_DIV2_HZ         (HSI_HZ/2UL)   /* 4 MHz */
#define SYSCLK_MAX_HZ       48000000UL

/* Wait por flag com timeout simples */
static bool wait_flag(volatile uint32_t *reg, uint32_t mask, bool set, uint32_t timeout) {
    while (timeout--) {
        if (set)  { if ((*reg & mask) == mask) return true; }
        else      { if ((*reg & mask) == 0u)   return true; }
    }
    return false;
}

/* ---- Básico ---- */
void rcc_reset_to_hsi(void) {
    /* Liga HSI */
    RCC->CR |= RCC_CR_HSION;
    (void)wait_flag(&RCC->CR, RCC_CR_HSIRDY, true, 1000000);

    /* Seleciona HSI como SYSCLK */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_HSI;
    (void)wait_flag(&RCC->CFGR, RCC_CFGR_SWS_Msk, false, 100000); // espera SWS=HSI (0)

    /* Prescalers = 1 */
    RCC->CFGR &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE_Msk);

    /* Desliga PLL e HSE (opcional) */
    RCC->CR &= ~RCC_CR_PLLON;
    (void)wait_flag(&RCC->CR, RCC_CR_PLLRDY, false, 100000);
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON);
    /* FLASH: 0 WS e prefetch ligado (opcional) */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_PRFTBE;
}

void rcc_config_flash_latency(uint32_t sysclk_hz) {
    if (sysclk_hz > 24000000UL) FLASH->ACR |= FLASH_ACR_LATENCY;
    else                         FLASH->ACR &= ~FLASH_ACR_LATENCY;
    /* Prefetch recomendado em frequências mais altas */
    FLASH->ACR |= FLASH_ACR_PRFTBE;
}

void rcc_set_prescalers(rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div) {
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE_Msk) | ((uint32_t)ahb_div << RCC_CFGR_HPRE_Pos);
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE_Msk) | ((uint32_t)apb_div << RCC_CFGR_PPRE_Pos);
}

bool rcc_switch_sysclk(rcc_sysclk_src_t src) {
    uint32_t sw = RCC_CFGR_SW_HSI;
    uint32_t expect = RCC_CFGR_SWS_HSI;

    if (src == RCC_SYSCLK_SRC_HSE) { sw = RCC_CFGR_SW_HSE; expect = RCC_CFGR_SWS_HSE; }
    else if (src == RCC_SYSCLK_SRC_PLL) { sw = RCC_CFGR_SW_PLL; expect = RCC_CFGR_SWS_PLL; }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | sw;
    /* espera SWS == expect */
    uint32_t timeout = 1000000;
    while (timeout--) {
        if ((RCC->CFGR & RCC_CFGR_SWS_Msk) == expect) return true;
    }
    return false;
}

bool rcc_enable_hse(bool bypass, bool enable_css) {
    if (bypass) RCC->CR |= RCC_CR_HSEBYP;
    else        RCC->CR &= ~RCC_CR_HSEBYP;

    RCC->CR |= RCC_CR_HSEON;
    if (!wait_flag(&RCC->CR, RCC_CR_HSERDY, true, 4000000)) return false;

    if (enable_css) RCC->CR |= RCC_CR_CSSON; /* HSE clock security system */
    return true;
}

bool rcc_enable_hsi(void) {
    RCC->CR |= RCC_CR_HSION;
    return wait_flag(&RCC->CR, RCC_CR_HSIRDY, true, 400000);
}

/* PLL: precondição — PLL OFF */
bool rcc_config_pll(rcc_pll_src_t src, uint32_t hse_hz, uint8_t prediv, uint8_t pllmul) {
    /* Garante PLL OFF */
    if (RCC->CR & RCC_CR_PLLON) {
        RCC->CR &= ~RCC_CR_PLLON;
        if (!wait_flag(&RCC->CR, RCC_CR_PLLRDY, false, 100000)) return false;
    }

    /* Seleciona fonte da PLL */
    if (src == RCC_PLL_SRC_HSI_DIV2) {
        RCC->CFGR &= ~RCC_CFGR_PLLSRC; // HSI/2
    } else {
        /* HSE/PREDIV */
        RCC->CFGR |= RCC_CFGR_PLLSRC;
        /* Configura PREDIV (1..16 => codifica 0..15) */
        if (prediv < 1) prediv = 1;
        if (prediv > 16) prediv = 16;
        uint32_t pv = (uint32_t)(prediv - 1);
        RCC->CFGR2 = (RCC->CFGR2 & ~RCC_CFGR2_PREDIV_Msk) | (pv << RCC_CFGR2_PREDIV_Pos);
    }

    /* Multiplicador PLLMUL (x2..x16: codifica 0..14 → x2..x16, 15 também x16 conforme linha F0) */
    if (pllmul < 2) pllmul = 2;
    if (pllmul > 16) pllmul = 16;
    uint32_t mul_code = (uint32_t)(pllmul - 2) & 0x0F;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PLLMUL_Msk) | (mul_code << RCC_CFGR_PLLMUL_Pos);

    /* Liga PLL e espera ready */
    RCC->CR |= RCC_CR_PLLON;
    return wait_flag(&RCC->CR, RCC_CR_PLLRDY, true, 500000);
}

/* ---- Helpers “alto nível” ---- */

/* HSI/2 → 4 MHz * PLLMUL = SYSCLK alvo (com HPRE=1). Só fecha se for exato e ≤ 48MHz. */
bool rcc_set_sysclk_from_hsi(uint32_t target_sysclk_hz,
                             rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div)
{
    if (target_sysclk_hz == 0 || target_sysclk_hz > SYSCLK_MAX_HZ) return false;

    /* Tenta fechar exato: mul = alvo / 4MHz */
    uint32_t mul = (target_sysclk_hz + (HSI_DIV2_HZ/2)) / HSI_DIV2_HZ; // arredonda
    if (mul < 2 || mul > 16) return false;
    if ((HSI_DIV2_HZ * mul) != target_sysclk_hz) return false;

    /* Liga HSI */
    if (!rcc_enable_hsi()) return false;

    /* Ajusta FLASH WS baseado no SYSCLK final pretendido */
    rcc_config_flash_latency(target_sysclk_hz);

    /* Prescalers primeiro (seguro reduzir frequência instantânea após switch) */
    rcc_set_prescalers(ahb_div, apb_div);

    /* Configura PLL HSI/2 * mul */
    if (!rcc_config_pll(RCC_PLL_SRC_HSI_DIV2, 0, 1, (uint8_t)mul)) return false;

    /* Comuta para PLL */
    return rcc_switch_sysclk(RCC_SYSCLK_SRC_PLL);
}

/* HSE → PREDIV (1..16) → PLLMUL (2..16). Se prediv_hint=0 tenta encontrar par que feche o alvo. */
bool rcc_set_sysclk_from_hse(uint32_t hse_hz, uint32_t target_sysclk_hz,
                             uint8_t prediv_hint,
                             rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div,
                             bool bypass, bool css_enable)
{
    if (hse_hz == 0 || target_sysclk_hz == 0 || target_sysclk_hz > SYSCLK_MAX_HZ) return false;

    if (!rcc_enable_hse(bypass, css_enable)) return false;

    /* Busca parâmetros */
    uint8_t best_prediv = 0, best_mul = 0;
    bool found = false;

    if (prediv_hint >= 1 && prediv_hint <= 16) {
        uint32_t fin = hse_hz / prediv_hint;
        for (uint8_t mul = 2; mul <= 16; ++mul) {
            if (fin * mul == target_sysclk_hz) { best_prediv = prediv_hint; best_mul = mul; found = true; break; }
        }
    } else {
        for (uint8_t pre = 1; pre <= 16 && !found; ++pre) {
            uint32_t fin = hse_hz / pre;
            for (uint8_t mul = 2; mul <= 16; ++mul) {
                if (fin * mul == target_sysclk_hz) { best_prediv = pre; best_mul = mul; found = true; break; }
            }
        }
    }

    if (!found) return false;

    /* Ajusta FLASH e prescalers antes do switch */
    rcc_config_flash_latency(target_sysclk_hz);
    rcc_set_prescalers(ahb_div, apb_div);

    /* Configura PLL e comuta */
    if (!rcc_config_pll(RCC_PLL_SRC_HSE_PREDIV, hse_hz, best_prediv, best_mul)) return false;
    return rcc_switch_sysclk(RCC_SYSCLK_SRC_PLL);
}

/* ---- Leitura das frequências atuais ---- */
static uint32_t decode_ahb_div(uint32_t hpre) {
    static const uint16_t map[16] = {1,1,1,1,1,1,1,1,2,4,8,16,64,128,256,512};
    return map[hpre & 0xF];
}
static uint32_t decode_apb_div(uint32_t ppre) {
    static const uint8_t map[8] = {1,1,1,1,2,4,8,16};
    return map[ppre & 0x7];
}
static uint32_t decode_pllmul(uint32_t cfgr) {
    uint32_t code = (cfgr >> RCC_CFGR_PLLMUL_Pos) & 0xF;
    uint32_t mul = (code + 2);
    if (mul > 16) mul = 16;
    return mul;
}
static uint32_t decode_prediv(uint32_t cfgr2) {
    return ((cfgr2 >> RCC_CFGR2_PREDIV_Pos) & 0xF) + 1;
}

void rcc_get_clocks(rcc_clocks_t *out) {
    if (!out) return;
    uint32_t cfgr = RCC->CFGR;
    uint32_t cfgr2 = RCC->CFGR2;

    uint32_t sws = (cfgr & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos;
    rcc_sysclk_src_t src = RCC_SYSCLK_SRC_HSI;
    uint32_t sysclk = HSI_HZ;

    if (sws == 0x0) { src = RCC_SYSCLK_SRC_HSI; sysclk = HSI_HZ; }
    else if (sws == 0x1) { src = RCC_SYSCLK_SRC_HSE; /* sem saber HSE exato — não temos LSI aqui */ }
    else if (sws == 0x2) {
        src = RCC_SYSCLK_SRC_PLL;
        uint32_t pll_src_hse = (cfgr & RCC_CFGR_PLLSRC) ? 1u : 0u;
        uint32_t mul = decode_pllmul(cfgr);
        if (pll_src_hse) {
            uint32_t pre = decode_prediv(cfgr2);
            (void)pre;
            /* Sem conhecer HSE externo em tempo de execução, não dá para inferir exato.
               Usuário pode medir via MCO/cronômetro ou guardar HSE_HZ em variável global. */
            /* Se você quiser, salve o HSE_HZ em uma global e use aqui. */
        } else {
            sysclk = HSI_DIV2_HZ * mul; /* HSI/2 * mul */
        }
    }

    uint32_t ahb_div = decode_ahb_div((cfgr >> RCC_CFGR_HPRE_Pos) & 0xF);
    uint32_t apb_div = decode_apb_div((cfgr >> RCC_CFGR_PPRE_Pos) & 0x7);

    out->sys_source = src;
    out->sysclk_hz  = sysclk / ahb_div; /* Tecnicamente SYSCLK → HPRE → HCLK */
    out->hclk_hz    = out->sysclk_hz;
    out->pclk_hz    = out->hclk_hz / apb_div;

    /* PLL infos best-effort (com HSE desconhecido pode ficar 0) */
    if ((cfgr & RCC_CFGR_PLLSRC) == 0) {
        out->pll_in_hz  = HSI_DIV2_HZ;
        out->pll_out_hz = HSI_DIV2_HZ * decode_pllmul(cfgr);
    } else {
        out->pll_in_hz  = 0;
        out->pll_out_hz = 0;
    }
}

/* ---- MCO (PA8) ----
 * Em F0: CFGR[26:24] MCO (SYSCLK/HSI/HSE/PLL/… depende da variante)
 *        CFGR[30:28] MCOPRE
 * Você pode consultar o RM do seu submodelo para as combinações válidas.
 */
void rcc_config_mco(uint32_t source_sel, uint32_t prescaler_sel) {
    /* Limpa campos e aplica */
    RCC->CFGR &= ~((7u << 24) | (7u << 28));
    RCC->CFGR |= ((source_sel & 7u) << 24) | ((prescaler_sel & 7u) << 28);
}

/* ====== LSI / LSE / RTC ====== */

void rcc_backup_domain_write_begin(void) {
    /* Liga clock de PWR e libera escrita no BDCR */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
}

void rcc_backup_domain_write_end(void) {
    /* Opcional: religa proteção; pode manter habilitada se for continuar no RTC */
    PWR->CR &= ~PWR_CR_DBP;
}

/* LSI ~40 kHz */
bool rcc_enable_lsi(uint32_t timeout) {
    RCC->CSR |= RCC_CSR_LSION;
    while (timeout--) {
        if (RCC->CSR & RCC_CSR_LSIRDY) return true;
    }
    return false;
}

void rcc_disable_lsi(void) {
    RCC->CSR &= ~RCC_CSR_LSION;
}

/* LSE 32.768 kHz (com ou sem bypass) */
bool rcc_enable_lse(bool bypass, uint32_t timeout) {
    rcc_backup_domain_write_begin();

    /* Se já estava ligado, desligue primeiro para ajustar bypass com segurança */
    if (RCC->BDCR & RCC_BDCR_LSEON) {
        RCC->BDCR &= ~RCC_BDCR_LSEON;
        /* aguarda apagar LSERDY (curto) */
        (void)wait_flag(&RCC->BDCR, RCC_BDCR_LSERDY, false, 100000);
    }

    if (bypass) RCC->BDCR |=  RCC_BDCR_LSEBYP;
    else        RCC->BDCR &= ~RCC_BDCR_LSEBYP;

    RCC->BDCR |= RCC_BDCR_LSEON;

    /* espera LSERDY */
    while (timeout--) {
        if (RCC->BDCR & RCC_BDCR_LSERDY) {
            rcc_backup_domain_write_end();
            return true;
        }
    }
    rcc_backup_domain_write_end();
    return false;
}

void rcc_disable_lse(void) {
    rcc_backup_domain_write_begin();
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    rcc_backup_domain_write_end();
}

/* Seleciona fonte do RTC (requer acesso ao backup domain) */
bool rcc_rtc_select_clock(rcc_rtc_clk_t src) {
    rcc_backup_domain_write_begin();

    /* Não pode mudar RTCSEL enquanto RTCEN=1 */
    if (RCC->BDCR & RCC_BDCR_RTCEN) {
        RCC->BDCR &= ~RCC_BDCR_RTCEN;
    }

    /* Limpa RTCSEL */
    RCC->BDCR &= ~RCC_BDCR_RTCSEL_Msk;

    switch (src) {
        case RCC_RTC_CLK_NONE:
            /* nada a fazer */
            break;
        case RCC_RTC_CLK_LSE:
            /* garante LSE pronto antes de selecionar */
            if (!(RCC->BDCR & RCC_BDCR_LSERDY)) { rcc_backup_domain_write_end(); return false; }
            RCC->BDCR |= (1u << RCC_BDCR_RTCSEL_Pos); /* 01: LSE */
            break;
        case RCC_RTC_CLK_LSI:
            /* garante LSI pronto antes de selecionar */
            if (!(RCC->CSR & RCC_CSR_LSIRDY)) { rcc_backup_domain_write_end(); return false; }
            RCC->BDCR |= (2u << RCC_BDCR_RTCSEL_Pos); /* 10: LSI */
            break;
        case RCC_RTC_CLK_HSE_DIV128:
            /* Para HSE/128, é necessário HSE ligado (não checamos aqui) */
            RCC->BDCR |= (3u << RCC_BDCR_RTCSEL_Pos); /* 11: HSE/128 */
            break;
        default:
            rcc_backup_domain_write_end();
            return false;
    }

    rcc_backup_domain_write_end();
    return true;
}

void rcc_rtc_enable(bool en) {
    rcc_backup_domain_write_begin();
    if (en)  RCC->BDCR |=  RCC_BDCR_RTCEN;
    else     RCC->BDCR &= ~RCC_BDCR_RTCEN;
    rcc_backup_domain_write_end();
}

/* ====== FACILITADORES (PRESETS) ====== */

bool rcc_preset_sysclk_8mhz_hsi(void) {
    /* Volta ao HSI base, WS=0, prescalers=1 */
    rcc_reset_to_hsi();
    /* Já estamos com SYSCLK=HSI=8 MHz */
    rcc_config_flash_latency(8000000UL);
    rcc_set_prescalers(RCC_AHB_DIV1, RCC_APB_DIV1);
    return rcc_switch_sysclk(RCC_SYSCLK_SRC_HSI);
}

bool rcc_preset_sysclk_24mhz_hsi(void) {
    /* HSI/2 = 4 MHz; 4*6 = 24 MHz */
    return rcc_set_sysclk_from_hsi(24000000UL, RCC_AHB_DIV1, RCC_APB_DIV1);
}

bool rcc_preset_sysclk_32mhz_hsi(void) {
    /* HSI/2 = 4 MHz; 4*8 = 32 MHz */
    return rcc_set_sysclk_from_hsi(32000000UL, RCC_AHB_DIV1, RCC_APB_DIV1);
}

bool rcc_preset_sysclk_48mhz_hsi(void) {
    /* HSI/2 = 4 MHz; 4*12 = 48 MHz (limite do F070) */
    return rcc_set_sysclk_from_hsi(48000000UL, RCC_AHB_DIV1, RCC_APB_DIV1);
}

bool rcc_preset_sysclk_48mhz_hse(uint32_t hse_hz, bool bypass, bool css) {
    /* Tenta montar 48 MHz a partir do HSE informado.
       Primeiro testamos PREDIV=1..16 e MUL=2..16 para fechar exato 48 MHz. */
    return rcc_set_sysclk_from_hse(hse_hz, 48000000UL,
                                   0 /* auto prediv */,
                                   RCC_AHB_DIV1, RCC_APB_DIV1,
                                   bypass, css);
}
