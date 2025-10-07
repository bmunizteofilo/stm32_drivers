#include "usart_poll.h"

/* Helpers de espera com timeout por ciclos (busy-wait) */
static inline bool wait_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_cycles) {
    while (timeout_cycles--) {
        if ((*reg & mask) != 0u) return true;
        __asm volatile ("nop");
    }
    return false;
}
static inline bool wait_flag_clr(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_cycles) {
    while (timeout_cycles--) {
        if ((*reg & mask) == 0u) return true;
        __asm volatile ("nop");
    }
    return false;
}

/* Liga clock da instância */
static void usart_enable_clock(USART_TypeDef *inst) {
    if (inst == USART1) RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    else if (inst == USART2) RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
}

/* Programa BRR de acordo com oversampling */
static void usart_set_baud(USART_TypeDef *us, uint32_t pclk_hz, uint32_t baud, uint8_t over8) {
    if (!over8) {
        /* OVER16: BRR = pclk/baud (inteiro, arredondado) */
        uint32_t brr = (pclk_hz + baud/2u) / baud;
        us->BRR = brr;
        us->CR1 &= ~(1u << 15); /* OVER8=0 */
    } else {
        /* OVER8: USARTDIV = pclk/(8*baud), BRR codifica mantissa e fração especial.
           Para simplicidade e robustez, usamos o modo OVER16 por padrão. */
        uint32_t usartdiv_x100 = (100u * pclk_hz + (4u*baud)) / (8u * baud);
        uint32_t mant = usartdiv_x100 / 100u;
        uint32_t frac = (usartdiv_x100 - mant*100u) * 8u / 100u; /* 3 bits [2:0] colocados em BRR[2:0] com bit3 pulado */
        /* Formato BRR (OVER8=1): mantissa em [15:4], frac em [2:0], bit3 zero */
        us->BRR = (mant << 4) | (frac & 0x7u);
        us->CR1 |= (1u << 15); /* OVER8=1 */
    }
}

void usart_poll_init(usart_poll_t *u, USART_TypeDef *inst, uint32_t pclk_hz,
                     const usart_poll_config_t *cfg)
{
    u->inst    = inst;
    u->pclk_hz = pclk_hz;
    u->cfg     = *cfg;

    usart_enable_clock(inst);

    USART_TypeDef *us = u->inst;

    /* Desliga enquanto configura */
    us->CR1 = 0;

    /* STOP bits */
    us->CR2 &= ~(3u << 12);
    us->CR2 |= ((uint32_t)cfg->stopbits << 12);

    /* Paridade e wordlen */
    uint32_t cr1 = 0;
    if (cfg->wordlen == USART_WORDLEN_9B) cr1 |= (1u << 12);        /* M=1 → 9 bits */
    if (cfg->parity == USART_PARITY_EVEN)      cr1 |= (1u << 10);   /* PCE */
    else if (cfg->parity == USART_PARITY_ODD)  cr1 |= (1u << 10) | (1u << 9); /* PCE|PS */

    /* Baud */
    usart_set_baud(us, pclk_hz, cfg->baud, cfg->oversample8 ? 1u : 0u);

    /* Habilita RX e TX (sem interrupções) */
    cr1 |= (1u << 2) | (1u << 3); /* RE|TE */
    us->CR1 = cr1;

    /* UE=1 (liga USART) */
    us->CR1 |= (1u << 0);
}

/* TX: espera TXE, escreve TDR, opcionalmente espera TC ao final no chamador */
bool usart_poll_write_byte(usart_poll_t *u, uint16_t data, uint32_t timeout_cycles)
{
    USART_TypeDef *us = u->inst;
    if (!wait_flag_set(&us->ISR, (1u<<7) /*TXE*/, timeout_cycles)) return false;
    us->TDR = (uint16_t)(data & ((u->cfg.wordlen==USART_WORDLEN_9B)?0x01FFu:0x00FFu));
    return true;
}

uint32_t usart_poll_write(usart_poll_t *u, const void *buf, uint32_t len, uint32_t timeout_cycles_per_byte)
{
    const uint8_t *p = (const uint8_t*)buf;
    uint32_t sent = 0;
    while (sent < len) {
        if (!usart_poll_write_byte(u, p[sent], timeout_cycles_per_byte)) break;
        sent++;
    }
    /* garante fim da transmissão (TC) do último byte */
    (void)wait_flag_set(&u->inst->ISR, (1u<<6) /*TC*/, timeout_cycles_per_byte);
    return sent;
}

uint32_t usart_poll_write_str(usart_poll_t *u, const char *s, uint32_t timeout_cycles_per_byte)
{
    uint32_t n = 0;
    while (*s) {
        if (!usart_poll_write_byte(u, (uint8_t)*s++, timeout_cycles_per_byte)) break;
        n++;
    }
    (void)wait_flag_set(&u->inst->ISR, (1u<<6), timeout_cycles_per_byte);
    return n;
}

/* RX: espera RXNE, lê RDR; trata erros básicos antes de retornar */
bool usart_poll_read_byte(usart_poll_t *u, uint16_t *out, uint32_t timeout_cycles)
{
    USART_TypeDef *us = u->inst;

    /* se houver erro pendente, limpe e descarte o byte anterior */
    uint32_t isr = us->ISR;
    if (isr & ((1u<<3)/*ORE*/|(1u<<2)/*NE*/|(1u<<1)/*FE*/|(1u<<0)/*PE*/)) {
        (void)us->RDR; /* leitura limpa ORE/PE na maioria dos casos; complete com ICR se necessário */
        us->ICR = (1u<<3)|(1u<<2)|(1u<<1)|(1u<<0);
    }

    if (!wait_flag_set(&us->ISR, (1u<<5) /*RXNE*/, timeout_cycles)) return false;

    uint16_t d = (uint16_t)us->RDR;
    if (u->cfg.wordlen == USART_WORDLEN_9B && u->cfg.parity==USART_PARITY_NONE) d &= 0x01FFu;
    else d &= 0x00FFu;

    if (out) *out = d;
    return true;
}

uint32_t usart_poll_read(usart_poll_t *u, void *buf, uint32_t len, uint32_t timeout_cycles_per_byte)
{
    uint8_t *p = (uint8_t*)buf;
    uint32_t got = 0;
    while (got < len) {
        uint16_t d;
        if (!usart_poll_read_byte(u, &d, timeout_cycles_per_byte)) break;
        p[got++] = (uint8_t)d; /* para 9 bits, ajuste conforme seu protocolo */
    }
    return got;
}

void usart_poll_clear_errors(usart_poll_t *u)
{
    USART_TypeDef *us = u->inst;
    (void)us->RDR; /* descarta dado */
    us->ICR = (1u<<3)|(1u<<2)|(1u<<1)|(1u<<0); /* ORECF|NCF|FECF|PECF */
}
