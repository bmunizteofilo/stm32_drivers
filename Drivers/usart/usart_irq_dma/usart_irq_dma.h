#ifndef STM32F070_USART_DRV_H
#define STM32F070_USART_DRV_H

#include "stm32f070xx.h"

/* ===== Config ===== */
typedef enum {
	UDRV_WORDLEN_8B = 0,
	UDRV_WORDLEN_9B = 1
} udrv_wordlen_t;
typedef enum {
	UDRV_PARITY_NONE = 0,
	UDRV_PARITY_EVEN = 1,
	UDRV_PARITY_ODD = 2
} udrv_parity_t;
typedef enum {
	UDRV_STOPBITS_1 = 0,
	UDRV_STOPBITS_2 = 2
} udrv_stopbits_t;
typedef enum {
	UDRV_ENGINE_IRQ = 0,
	UDRV_ENGINE_DMA = 1
} udrv_engine_t;

typedef struct {
  uint32_t        baud;
  udrv_wordlen_t  wordlen;
  udrv_parity_t   parity;
  udrv_stopbits_t stopbits;
  uint8_t         oversample8;     /* 0=16x, 1=8x */

  udrv_engine_t   rx_engine;       /* IRQ ou DMA (DMA usa buffer circular + IDLE) */
  udrv_engine_t   tx_engine;       /* IRQ (ring) ou DMA (ring→rajadas) */

  uint8_t         nvic_prio_usart; /* 0..3 (Cortex-M0: 2 MSBs efetivos) */
} usart_drv_config_t;

/* Ring (potência de 2) */
typedef struct { volatile uint32_t head, tail, size; uint8_t *buf; } udrv_ring_t;

/* Handle */
typedef struct {
  USART_TypeDef *inst;
  uint32_t       pclk_hz;
  usart_drv_config_t cfg;

  /* RX */
  /* IRQ: ring; DMA: buffer linear circular + IDLE */
  udrv_ring_t rx_rb;                 /* válido só quando RX=IRQ */
  uint8_t    *rx_dma_buf;            /* quando RX=DMA */
  uint32_t    rx_dma_size;
  DMA_Channel_TypeDef *dma_rx;
  uint8_t     rx_ch_idx;             /* 1..7 para dma_router */
  volatile uint32_t rx_dma_last;     /* CNDTR anterior (para delta) */

  /* TX */
  udrv_ring_t tx_rb;                 /* sempre ring */
  DMA_Channel_TypeDef *dma_tx;       /* quando TX=DMA (rajadas) */
  uint8_t     tx_ch_idx;             /* 1..7 para dma_router */
  volatile uint16_t tx_dma_len;      /* bytes da rajada atual */

  /* Callbacks */
  void (*on_rx_chunk)(const uint8_t *data, uint32_t len); /* somente RX=DMA+IDLE */
  void (*on_tx_done)(void);
  void (*on_error)(uint32_t usart_isr, uint32_t dma_flags);
} usart_drv_t;

/* ===== API ===== */
void usart_init(usart_drv_t *u, USART_TypeDef *inst, uint32_t pclk_hz,
                const usart_drv_config_t *cfg,
                /* RX:
                   - RX=IRQ  → passe ring (buf,len potências de 2)
                   - RX=DMA  → passe rx_dma_buf e tamanho do buffer circular */
                uint8_t *rx_ring_or_dma_buf, uint32_t rx_len,
                /* TX ring (potência de 2) */
                uint8_t *tx_ring, uint32_t tx_len);

void usart_set_callbacks(usart_drv_t *u,
                         void (*on_rx_chunk)(const uint8_t*, uint32_t),
                         void (*on_tx_done)(void),
                         void (*on_error)(uint32_t, uint32_t));

/* Escrita (ring) — dispara por IRQ (TXE) ou por DMA (rajadas) */
uint32_t usart_write(usart_drv_t *u, const void *data, uint32_t len);

/* Leitura do RX ring (somente RX=IRQ). Retorna bytes lidos. */
uint32_t usart_read(usart_drv_t *u, void *out, uint32_t maxlen);

/* Flush TX (espera esvaziar e TC) */
void usart_flush(usart_drv_t *u);

void USART1_IRQHandler(void);
void USART2_IRQHandler(void);

#endif /* STM32F070_USART_DRV_H */
