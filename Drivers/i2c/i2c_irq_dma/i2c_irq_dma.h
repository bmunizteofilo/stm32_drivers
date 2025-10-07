#ifndef I2C_IRQ_DMA_H
#define I2C_IRQ_DMA_H

#include "stm32f070xx.h"
#include "i2c_poll.h"     /* base de registradores/defines */

/* ===== Erros ===== */
typedef enum {
    I2C_XFER_OK = 0,
    I2C_XFER_ERR_NACK,
    I2C_XFER_ERR_BERR,
    I2C_XFER_ERR_ARLO,
    I2C_XFER_ERR_OVR,
    I2C_XFER_ERR_TIMEOUT,
    I2C_XFER_ERR_PARAM,
    I2C_XFER_ERR_DMA,
} i2c_xfer_err_t;

/* ===== Estado ===== */
typedef enum {
    I2C_ST_IDLE = 0,
    I2C_ST_WRITE,
    I2C_ST_RESTART_FOR_READ,
    I2C_ST_READ,
    I2C_ST_DONE,
    I2C_ST_ERROR
} i2c_st_t;

/* ===== Contexto do driver ===== */
typedef struct i2c_drv_s {
    I2C_TypeDef *i2c;
    uint8_t dma_ch_tx;   /* mem->periph (TXDR) */
    uint8_t dma_ch_rx;   /* periph->mem (RXDR) */

    volatile i2c_st_t       st;
    volatile i2c_xfer_err_t err;
    uint8_t  addr7;

    const uint8_t *wbuf;
    size_t   wlen, wpos;

    uint8_t *rbuf;
    size_t   rlen, rpos;

    uint8_t  use_dma_tx;
    uint8_t  use_dma_rx;

    size_t   cur_chunk;     /* bytes restantes no chunk corrente */

    volatile uint8_t done;

    void (*on_complete)(struct i2c_drv_s *drv, i2c_xfer_err_t err, void *ctx);
    void *cb_ctx;
} i2c_drv_t;

/* Aliás opcional */
typedef i2c_drv_t i2c_drv_handle_t;

/* ===== API ===== */
void i2c_irqdma_init(i2c_drv_t *d, I2C_TypeDef *i2c,
                     uint32_t timingr,
                     uint8_t analog_filter_en,
                     uint8_t digital_filter,
                     uint8_t own7bit_addr_or_0,
                     uint8_t irq_prio_0to3);

void i2c_irqdma_set_dma_channels(i2c_drv_t *d, uint8_t ch_tx, uint8_t ch_rx);

bool i2c_irqdma_start(i2c_drv_t *d, uint8_t addr7,
                      const uint8_t *wbuf, size_t wlen,
                      uint8_t *rbuf, size_t rlen,
                      uint8_t use_dma_tx, uint8_t use_dma_rx);

bool i2c_irqdma_wait_done(i2c_drv_t *d, uint32_t loop_timeout);

static inline void i2c_irqdma_set_callback(i2c_drv_t *d,
    void (*cb)(i2c_drv_t*, i2c_xfer_err_t, void*), void *ctx)
{ d->on_complete = (void(*)(struct i2c_drv_s*, i2c_xfer_err_t, void*))cb; d->cb_ctx = ctx; }

/* ISRs (exporte estes nomes no vetor) */
void I2C1_IRQHandler(void);
void I2C2_IRQHandler(void);

#endif /* I2C_IRQ_DMA_H */
