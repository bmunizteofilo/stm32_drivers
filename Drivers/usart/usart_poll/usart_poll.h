#ifndef __USART_POLL_H__
#define __USART_POLL_H__

#include "stm32f070xx.h"

/* ===== Config ===== */
typedef enum {
    USART_WORDLEN_8B = 0,
    USART_WORDLEN_9B = 1
} usart_wordlen_t;

typedef enum {
    USART_PARITY_NONE = 0,
    USART_PARITY_EVEN = 1,
    USART_PARITY_ODD  = 2
} usart_parity_t;

typedef enum {
    USART_STOPBITS_1 = 0,
    USART_STOPBITS_2 = 2      /* STOP[13:12] = 10 */
} usart_stopbits_t;

typedef struct {
    uint32_t       baud;
    usart_wordlen_t wordlen;   /* 8B ou 9B */
    usart_parity_t  parity;    /* None/Even/Odd */
    usart_stopbits_t stopbits; /* 1 ou 2 */
    uint8_t        oversample8;/* 0 = over16 (recomendado), 1 = over8 */
} usart_poll_config_t;

typedef struct {
    USART_TypeDef *inst;
    uint32_t       pclk_hz;    /* clock do barramento onde a USART está */
    usart_poll_config_t cfg;
} usart_poll_t;

/* ===== API ===== */
void usart_poll_init(usart_poll_t *u, USART_TypeDef *inst, uint32_t pclk_hz,
                     const usart_poll_config_t *cfg);

/* Envia um byte (bloqueante). Retorna true se enviado. */
bool usart_poll_write_byte(usart_poll_t *u, uint16_t data, uint32_t timeout_cycles);

/* Envia buffer inteiro (bloqueante). Retorna bytes enviados. */
uint32_t usart_poll_write(usart_poll_t *u, const void *buf, uint32_t len, uint32_t timeout_cycles_per_byte);

/* Envia string (terminada em '\0'). Retorna chars enviados (sem o '\0'). */
uint32_t usart_poll_write_str(usart_poll_t *u, const char *s, uint32_t timeout_cycles_per_byte);

/* Recebe um byte (bloqueante). Retorna true e preenche *out. */
bool usart_poll_read_byte(usart_poll_t *u, uint16_t *out, uint32_t timeout_cycles);

/* Recebe até len bytes (bloqueante por byte). Retorna quantidade lida. */
uint32_t usart_poll_read(usart_poll_t *u, void *buf, uint32_t len, uint32_t timeout_cycles_per_byte);

/* Limpa flags de erro e dados pendentes no RDR. */
void usart_poll_clear_errors(usart_poll_t *u);

#endif /* __USART_POLL_H__ */
