/*
 * spi_irq_dma.h
 *
 *  Created on: Aug 14, 2025
 *      Author: Bruno
 */

#ifndef __SPI_IRQ_DMA_H__
#define __SPI_IRQ_DMA_H__

#include "stm32f070xx.h"
#include "spi_poll.h"
/* ===== Configuração ===== */

typedef enum {
	SPI_ENGINE_IRQ = 0,
	SPI_ENGINE_DMA = 1
} spi_engine_t;

typedef struct {
  /* núcleo SPI */
  spi_mode_t      mode;
  spi_baud_t      baud_div;
  spi_bit_order_t bit_order;
  spi_datasize_t  datasize;

  /* NSS */
  spi_nss_mode_t  nss_mode;      /* SOFT → usa callbacks de CS; HARD_AUTO → SSOE */
  uint8_t         nssp_pulse;    /* 1: CR2.NSSP (se suportado) */

  /* seleção do “engine” */
  spi_engine_t    tx_engine;     /* IRQ ou DMA */
  spi_engine_t    rx_engine;     /* IRQ ou DMA */

  /* prioridade da IRQ do SPI (DMA é do dma_router_init) */
  uint8_t         nvic_prio_spi; /* 0..3 (Cortex-M0: 2 MSBs efetivos) */

  /* callbacks de CS (só no NSS_SOFT) */
  void (*cs_assert)(void);
  void (*cs_release)(void);
} spi_drv_config_t;

/* ===== Handle ===== */
typedef struct {
  SPI_TypeDef *inst;
  spi_drv_config_t cfg;

  /* estado da transação corrente */
  volatile uint8_t  busy;
  volatile uint8_t  bytes_per_item; /* 1 (8b) ou 2 (16b) */

  const void *tx_buf;
  void       *rx_buf;
  uint32_t    count;               /* nº de itens (8/16 bits) */
  volatile uint32_t tx_idx;        /* contadores para IRQ */
  volatile uint32_t rx_idx;

  /* DMA channels & flags */
  DMA_Channel_TypeDef *dma_rx;
  DMA_Channel_TypeDef *dma_tx;
  uint8_t rx_ch_idx;               /* 1..7 (para o dma_router) */
  uint8_t tx_ch_idx;
  volatile uint8_t rx_dma_active;  /* se RX DMA está ativo nesta transação */
  volatile uint8_t tx_dma_active;  /* se TX DMA está ativo nesta transação */
  volatile uint8_t dma_rx_done;    /* setado pelo callback do router */
  volatile uint8_t dma_tx_done;

  /* dummies/sumidouros p/ modos only */
  uint16_t tx_dummy16; uint8_t tx_dummy8;
  uint16_t rx_discard16; uint8_t rx_discard8;

  /* callbacks do usuário */
  void (*on_complete)(void);
  void (*on_error)(uint32_t sr, uint32_t dma_flags);

  /* ===== Multi-fase (write-then-read com CS baixo em NSS_SOFT) ===== */
  uint8_t  multi_active;      /* 1=transação 2-fases ativa */
  uint8_t  phase;             /* 1 ou 2 */
  const void *phase2_tx;      /* normalmente NULL (dummy) */
  void      *phase2_rx;
  uint32_t   phase2_count;

  /* guardamos callbacks do usuário para chamar só ao fim da última fase */
  void (*user_on_complete)(void);
  void (*user_on_error)(uint32_t, uint32_t);
} spi_drv_t;

/* ===== API ===== */
void spi_init(spi_drv_t *s, SPI_TypeDef *inst, const spi_drv_config_t *cfg);

/* inicia transação (full-duplex). Qualquer lado pode ser NULL.
   count = nº de itens (bytes se 8b, halfwords se 16b).
   Retorna false se já estiver ocupado. */
bool spi_transfer_async(spi_drv_t *s, const void *tx, void *rx, uint32_t count);

/* utilidades */
static inline bool spi_is_busy(const spi_drv_t *s){ return s->busy != 0; }
void spi_wait(spi_drv_t *s);
void spi_abort(spi_drv_t *s);

void spi_set_callbacks(spi_drv_t *s, void (*on_complete)(void),
                                  void (*on_error)(uint32_t,uint32_t));

/* Mantém CS baixo (NSS_SOFT) entre as duas fases.
   Fase 1: envia tx1 (n_tx1 itens), RX descartado
   Fase 2: lê rx2 (n_rx2 itens), TX dummy
   Retorna false se o SPI estiver ocupado. */
bool spi_write_then_read_async(spi_drv_t *s,
                               const void *tx1, uint32_t n_tx1,
                               void *rx2, uint32_t n_rx2);


void SPI1_IRQHandler(void);
void SPI2_IRQHandler(void);

#endif /* __SPI_IRQ_DMA_H__ */
