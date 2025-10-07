#ifndef __RCC_H__
#define __RCC_H__

#include "stm32f070xx.h"

/* ---------------- Tipos e API ---------------- */

typedef enum {
    RCC_SYSCLK_SRC_HSI = 0,
    RCC_SYSCLK_SRC_HSE = 1,
    RCC_SYSCLK_SRC_PLL = 2
} rcc_sysclk_src_t;

typedef enum {
    RCC_PLL_SRC_HSI_DIV2 = 0,   // entrada 4 MHz
    RCC_PLL_SRC_HSE_PREDIV = 1  // entrada = HSE / PREDIV
} rcc_pll_src_t;

typedef struct {
    uint32_t sysclk_hz;  // = HCLK
    uint32_t hclk_hz;    // AHB
    uint32_t pclk_hz;    // APB
    uint32_t pll_in_hz;  // entrada da PLL (após HSI/2 ou HSE/PREDIV)
    uint32_t pll_out_hz; // PLL VCO (após multiplicador) = SYSCLK quando SW=PLL e HPRE=1
    rcc_sysclk_src_t sys_source;
} rcc_clocks_t;

/* Prescalers AHB (HPRE) */
typedef enum {
    RCC_AHB_DIV1   = 0x0,
    RCC_AHB_DIV2   = 0x8,
    RCC_AHB_DIV4   = 0x9,
    RCC_AHB_DIV8   = 0xA,
    RCC_AHB_DIV16  = 0xB,
    RCC_AHB_DIV64  = 0xC,
    RCC_AHB_DIV128 = 0xD,
    RCC_AHB_DIV256 = 0xE,
    RCC_AHB_DIV512 = 0xF,
} rcc_ahb_div_t;

/* Prescalers APB (PPRE) */
typedef enum {
    RCC_APB_DIV1 = 0x0,
    RCC_APB_DIV2 = 0x4,
    RCC_APB_DIV4 = 0x5,
    RCC_APB_DIV8 = 0x6,
    RCC_APB_DIV16= 0x7,
} rcc_apb_div_t;

/* Seleção de clock do RTC */
typedef enum {
    RCC_RTC_CLK_NONE      = 0, /* RTC desligado/sem clock */
    RCC_RTC_CLK_LSE       = 1,
    RCC_RTC_CLK_LSI       = 2,
    RCC_RTC_CLK_HSE_DIV128= 3
} rcc_rtc_clk_t;

/* === Funções principais === */

/* Reseta configuração básica de clock (volta para HSI @ 8 MHz, prescalers = 1). */
void rcc_reset_to_hsi(void);

/* Configura FLASH wait states conforme SYSCLK (0 WS <=24MHz; 1 WS >24MHz). */
void rcc_config_flash_latency(uint32_t sysclk_hz);

/* Configura prescalers AHB/APB. */
void rcc_set_prescalers(rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div);

/* Seleciona fonte do SYSCLK (HSI/HSE/PLL) e espera comutação. */
bool rcc_switch_sysclk(rcc_sysclk_src_t src);

/* Liga HSE (com ou sem bypass) e aguarda pronto; retorna sucesso/fracasso. */
bool rcc_enable_hse(bool bypass, bool enable_css);

/* Liga HSI e aguarda pronto. */
bool rcc_enable_hsi(void);

/* Desliga PLL (espera) e liga PLL com parâmetros; não comuta SYSCLK automaticamente. */
bool rcc_config_pll(rcc_pll_src_t src, uint32_t hse_hz, uint8_t prediv, uint8_t pllmul);

/* Helper “auto” para alvo com HSI/2 → tenta fechar exato o SYSCLK desejado (até 48 MHz). */
bool rcc_set_sysclk_from_hsi(uint32_t target_sysclk_hz,
                             rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div);

/* Helper “auto” para alvo com HSE/PREDIV. */
bool rcc_set_sysclk_from_hse(uint32_t hse_hz, uint32_t target_sysclk_hz,
                             uint8_t prediv_hint,   /* 1..16 (0=auto) */
                             rcc_ahb_div_t ahb_div, rcc_apb_div_t apb_div,
                             bool bypass, bool css_enable);

/* Lê frequências atuais. */
void rcc_get_clocks(rcc_clocks_t *out);

/* Configura MCO (PA8) para sair SYSCLK/HSI/HSE/PLLCLK com divisor. */
void rcc_config_mco(uint32_t source_sel, uint32_t prescaler_sel); // fonte: CFGR[26:24], div: CFGR[30:28] (em F0: MCO/MCOPRE)

/* --- API LSI / LSE / RTC --- */
bool rcc_enable_lsi(uint32_t timeout);
void rcc_disable_lsi(void);

bool rcc_enable_lse(bool bypass, uint32_t timeout);
void rcc_disable_lse(void);

/* Habilita escrita no domínio de backup (BDCR) e depois religa proteção */
void rcc_backup_domain_write_begin(void);
void rcc_backup_domain_write_end(void);

/* Seleciona clock do RTC e habilita/desabilita o RTC */
bool rcc_rtc_select_clock(rcc_rtc_clk_t src);
void rcc_rtc_enable(bool en);

/* --- FACILITADORES (PRESETS) --- */
/* Todos retornam true em sucesso; configuram FLASH WS, prescalers e fazem o switch */
bool rcc_preset_sysclk_8mhz_hsi(void);   /* SYSCLK = 8 MHz (HSI direto) */
bool rcc_preset_sysclk_24mhz_hsi(void);  /* HSI/2 * 6 = 24 MHz */
bool rcc_preset_sysclk_32mhz_hsi(void);  /* HSI/2 * 8 = 32 MHz */
bool rcc_preset_sysclk_48mhz_hsi(void);  /* HSI/2 * 12 = 48 MHz */

/* A partir de HSE conhecido (ex.: 8 MHz → 48 MHz com PREDIV=1, MUL=6) */
bool rcc_preset_sysclk_48mhz_hse(uint32_t hse_hz, bool bypass, bool css);

#endif /* __RCC_H__ */
