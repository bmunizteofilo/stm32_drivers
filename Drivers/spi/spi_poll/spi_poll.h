/*
 * spi_poll.h
 *
 *  Created on: Aug 14, 2025
 *      Author: Bruno
 */

#ifndef __SPI_POLL_H__
#define __SPI_POLL_H__

#include "stm32f070xx.h"

/* ===== Config ===== */
typedef enum {
	SPI_MODE0 = 0,
	SPI_MODE1 = 1,
	SPI_MODE2 = 2,
	SPI_MODE3 = 3
} spi_mode_t;

/* Prescaler = HCLK/APBx / 2^(1..8) -> BR[2:0] = 000..111 */
typedef enum {
	SPI_BR_DIV2 = 0,
	SPI_BR_DIV4 = 1,
	SPI_BR_DIV8 = 2,
	SPI_BR_DIV16 = 3,
	SPI_BR_DIV32 = 4,
	SPI_BR_DIV64 = 5,
	SPI_BR_DIV128 = 6,
	SPI_BR_DIV256 = 7
} spi_baud_t;

typedef enum {
	SPI_MSB_FIRST = 0,
	SPI_LSB_FIRST = 1
} spi_bit_order_t;

typedef enum {
	SPI_DS_8BIT = 8,
	SPI_DS_16BIT = 16
} spi_datasize_t;

/* NOVO: modo de NSS */
typedef enum {
  SPI_NSS_SOFT = 0,     /* SSM/SSI; CS por GPIO (callbacks abaixo) */
  SPI_NSS_HARD_AUTO = 1 /* SSOE; NSS sai no pino AF automaticamente */
} spi_nss_mode_t;

typedef struct {
  spi_mode_t       mode;          /* CPOL/CPHA */
  spi_baud_t       baud_div;      /* prescaler */
  spi_bit_order_t  bit_order;     /* MSB/LSB */
  spi_datasize_t   datasize;      /* 8 ou 16 */
  /* ===== NSS ===== */
  spi_nss_mode_t   nss_mode;      /* SOFT (SSM/SSI) ou HARD_AUTO (SSOE) */
  uint8_t          nssp_pulse;    /* 1 = habilita NSSP (pulso de NSS) quando suportado */
  /* Callbacks opcionais p/ controlar CS (ativo em 0 normalmente) */
  void (*cs_assert)(void);
  void (*cs_release)(void);
} spi_poll_config_t;

typedef struct {
  SPI_TypeDef     *inst;
  spi_poll_config_t cfg;
} spi_poll_t;

/* ===== API ===== */
void spi_poll_init(spi_poll_t *s, SPI_TypeDef *inst, const spi_poll_config_t *cfg);

/* Transferência full-duplex (polling).
   - Se tx==NULL: envia 0xFF (8b) / 0xFFFF (16b).
   - Se rx==NULL: lê e descarta.
   Retorna itens transferidos (bytes ou words, conforme datasize). */
uint32_t spi_poll_transfer(spi_poll_t *s,
                           const void *tx, void *rx, uint32_t count,
                           uint32_t timeout_cycles_per_item);

/* Atalhos: write-only e read-only (dummy = 0xFF/0xFFFF) */
uint32_t spi_poll_write(spi_poll_t *s, const void *tx, uint32_t count, uint32_t timeout_cycles_per_item);
uint32_t spi_poll_read (spi_poll_t *s, void *rx, uint32_t count, uint32_t timeout_cycles_per_item);

/* Helpers de CS (se você preferir chamar manualmente) */
static inline void spi_cs_assert(spi_poll_t *s){ if (s->cfg.cs_assert)  s->cfg.cs_assert(); }
static inline void spi_cs_release(spi_poll_t *s){ if (s->cfg.cs_release) s->cfg.cs_release(); }

/* Limpa erros (OVR/MODF/CRCERR) e esvazia FIFO */
void spi_poll_clear_errors(spi_poll_t *s);

#endif /* __SPI_POLL_H__ */
