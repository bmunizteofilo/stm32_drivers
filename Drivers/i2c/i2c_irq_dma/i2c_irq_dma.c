#include "i2c_irq_dma.h"

/* ===== Ponteiros estáticos por instância ===== */
static i2c_drv_t *s_i2c1 = 0;
static i2c_drv_t *s_i2c2 = 0;

/* ===== Helpers ===== */
static inline void i2c_enable_clock(I2C_TypeDef *i2c){ i2c_poll_enable_clock(i2c); }
static inline void i2c_soft_reset(I2C_TypeDef *i2c){ i2c_poll_reset(i2c); }

static i2c_xfer_err_t i2c_check_error_and_clear(I2C_TypeDef *i2c){
    uint32_t isr = i2c->ISR;
    if (isr & I2C_ISR_NACKF){ i2c->ICR = I2C_ICR_NACKCF; return I2C_XFER_ERR_NACK; }
    if (isr & I2C_ISR_BERR ){ i2c->ICR = I2C_ICR_BERRCF; return I2C_XFER_ERR_BERR; }
    if (isr & I2C_ISR_ARLO ){ i2c->ICR = I2C_ICR_ARLOCF; return I2C_XFER_ERR_ARLO; }
    if (isr & I2C_ISR_OVR  ){ i2c->ICR = I2C_ICR_OVRCF;  return I2C_XFER_ERR_OVR; }
    if (isr & I2C_ISR_TIMEOUT){ i2c->ICR = I2C_ICR_TIMOUTCF; return I2C_XFER_ERR_TIMEOUT; }
    return I2C_XFER_OK;
}
static inline void i2c_issue_stop(I2C_TypeDef *i2c){ i2c->CR2 |= I2C_CR2_STOP; }

static void i2c_prog_cr2_chunk(I2C_TypeDef *i2c, uint8_t addr7,
                               size_t nbytes, int read, int autoend, int reload)
{
    if (nbytes == 0) nbytes = 1; /* segurança */
    uint32_t cr2 = i2c->CR2;
    cr2 &= ~( (0x3FFu) | (1u<<10) | (0xFFu<<16) | I2C_CR2_AUTOEND | I2C_CR2_RELOAD | I2C_CR2_ADD10 );
    cr2 |= I2C_CR2_SADD_7(addr7 & 0x7F);
    if (read) cr2 |= I2C_CR2_RD_WRN;
    cr2 |= I2C_CR2_NBYTES((uint32_t)nbytes & 0xFFu);
    if (autoend) cr2 |= I2C_CR2_AUTOEND;
    if (reload)  cr2 |= I2C_CR2_RELOAD;
    cr2 |= I2C_CR2_START;
    i2c->CR2 = cr2;
}

static void i2c_finish(i2c_drv_t *d, i2c_xfer_err_t err)
{
    d->err  = err;
    d->st   = (err==I2C_XFER_OK) ? I2C_ST_DONE : I2C_ST_ERROR;

    /* desliga DMA se estava ligado */
    if (d->use_dma_tx) d->i2c->CR1 &= ~I2C_CR1_TxDMAEN;
    if (d->use_dma_rx) d->i2c->CR1 &= ~I2C_CR1_RxDMAEN;

    d->done = 1;
    if (d->on_complete) d->on_complete(d, err, d->cb_ctx);
}

/* DMA TX: mem->periph 8-bit → TXDR */
static bool i2c_dma_start_tx(i2c_drv_t *d, const uint8_t *buf, size_t len)
{
    if (d->dma_ch_tx < 1 || d->dma_ch_tx > 7) return false;

    dma_router_chan_cfg_t cfg = {
        .mem_to_periph = 1,
        .circular = 0,
        .minc = 1,
        .pinc = 0,
        .msize_bits = 0,  /* 8-bit */
        .psize_bits = 0,  /* 8-bit */
        .priority   = 2,  /* HIGH */
        .irq_tc = 0, .irq_ht = 0, .irq_te = 1
    };
    if (!dma_router_start(d->dma_ch_tx,
                          (uint32_t)&d->i2c->TXDR,
                          (uint32_t)buf,
                          (uint16_t)len, &cfg)) return false;

    d->i2c->CR1 |= I2C_CR1_TxDMAEN;
    return true;
}

/* DMA RX: periph->mem 8-bit ← RXDR */
static bool i2c_dma_start_rx(i2c_drv_t *d, uint8_t *buf, size_t len)
{
    if (d->dma_ch_rx < 1 || d->dma_ch_rx > 7) return false;

    dma_router_chan_cfg_t cfg = {
        .mem_to_periph = 0,
        .circular = 0,
        .minc = 1,
        .pinc = 0,
        .msize_bits = 0,  /* 8-bit */
        .psize_bits = 0,  /* 8-bit */
        .priority   = 2,
        .irq_tc = 0, .irq_ht = 0, .irq_te = 1
    };
    if (!dma_router_start(d->dma_ch_rx,
                          (uint32_t)&d->i2c->RXDR,
                          (uint32_t)buf,
                          (uint16_t)len, &cfg)) return false;

    d->i2c->CR1 |= I2C_CR1_RxDMAEN;
    return true;
}

/* ===== Init ===== */
void i2c_irqdma_init(i2c_drv_t *d, I2C_TypeDef *i2c,
                     uint32_t timingr,
                     uint8_t analog_filter_en,
                     uint8_t digital_filter,
                     uint8_t own7bit,
                     uint8_t irq_prio)
{
    d->i2c = i2c;
    d->dma_ch_tx = 0;
    d->dma_ch_rx = 0;
    d->st  = I2C_ST_IDLE; d->err = I2C_XFER_OK; d->done = 0;
    d->on_complete = 0; d->cb_ctx = 0;

    i2c_enable_clock(i2c);
    i2c_poll_init(&(i2c_poll_cfg_t){
        .inst = i2c,
        .timingr = timingr,
        .analog_filter_en = analog_filter_en,
        .digital_filter   = digital_filter,
        .own7bit          = own7bit
    });

    /* Habilita eventos/erros no periférico */
    d->i2c->CR1 |= I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE |
                   I2C_CR1_TCIE | I2C_CR1_ERRIE | I2C_CR1_NACKIE;

    /* Registra instância e habilita NVIC nos índices do seu enum */
    if (i2c == I2C1){ s_i2c1 = d; nvic_enable_irq(I2C1_IRQn, irq_prio); }
    else if (i2c == I2C2){ s_i2c2 = d; nvic_enable_irq(I2C2_IRQn, irq_prio); }
}

/* ===== Seleção de canais DMA ===== */
void i2c_irqdma_set_dma_channels(i2c_drv_t *d, uint8_t ch_tx, uint8_t ch_rx)
{
    d->dma_ch_tx = ch_tx;
    d->dma_ch_rx = ch_rx;
}

/* ===== Start ===== */
static inline size_t min_sz(size_t a, size_t b){ return (a<b)?a:b; }

bool i2c_irqdma_start(i2c_drv_t *d, uint8_t addr7,
                      const uint8_t *wbuf, size_t wlen,
                      uint8_t *rbuf, size_t rlen,
                      uint8_t use_dma_tx, uint8_t use_dma_rx)
{
    if (d->st != I2C_ST_IDLE) return false;
    if (!wlen && !rlen) return false;
    if ((use_dma_tx && (!d->dma_ch_tx)) || (use_dma_rx && (!d->dma_ch_rx))) return false;

    d->addr7 = addr7 & 0x7F;
    d->wbuf = wbuf; d->wlen = wlen; d->wpos = 0;
    d->rbuf = rbuf; d->rlen = rlen; d->rpos = 0;
    d->use_dma_tx = (use_dma_tx!=0);
    d->use_dma_rx = (use_dma_rx!=0);
    d->err = I2C_XFER_OK; d->done = 0;

    /* limpa flags */
    d->i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF|I2C_ICR_ADDRCF;

    /* WRITE primeiro? */
    if (wlen){
        d->st = I2C_ST_WRITE;
        size_t chunk = min_sz(wlen, 255);
        d->cur_chunk = chunk;

        if (d->use_dma_tx){
            if (!i2c_dma_start_tx(d, &d->wbuf[d->wpos], chunk)){
                i2c_finish(d, I2C_XFER_ERR_DMA); return false;
            }
        }
        int reload  = (wlen > chunk);
        int autoend = (!reload && rlen==0);
        i2c_prog_cr2_chunk(d->i2c, d->addr7, chunk, /*read=*/0, autoend, reload);
        return true;
    }

    /* só READ */
    d->st = I2C_ST_READ;
    size_t chunk = min_sz(rlen, 255);
    d->cur_chunk = chunk;

    if (d->use_dma_rx){
        if (!i2c_dma_start_rx(d, &d->rbuf[d->rpos], chunk)){
            i2c_finish(d, I2C_XFER_ERR_DMA); return false;
        }
    }
    int reload  = (rlen > chunk);
    int autoend = (!reload);
    i2c_prog_cr2_chunk(d->i2c, d->addr7, chunk, /*read=*/1, autoend, reload);
    return true;
}

/* ===== Espera busy-wait simples ===== */
bool i2c_irqdma_wait_done(i2c_drv_t *d, uint32_t loop_timeout)
{
    while (!d->done && loop_timeout){ loop_timeout--; }
    return (d->done && d->err == I2C_XFER_OK);
}

/* ===== Núcleo da ISR ===== */
static void i2c_isr_core(i2c_drv_t *d)
{
    I2C_TypeDef *i2c = d->i2c;
    uint32_t isr = i2c->ISR;

    /* Erros/NACK primeiro */
    if (isr & (I2C_ISR_NACKF|I2C_ISR_BERR|I2C_ISR_ARLO|I2C_ISR_OVR|I2C_ISR_TIMEOUT)){
        i2c_xfer_err_t e = i2c_check_error_and_clear(i2c);
        i2c_issue_stop(i2c);
        if (i2c->ISR & I2C_ISR_STOPF) i2c->ICR = I2C_ICR_STOPCF;
        i2c_finish(d, (e==I2C_XFER_OK)? I2C_XFER_ERR_BERR : e);
        return;
    }

    /* TX via IRQ (quando não DMA) */
    if ((d->st == I2C_ST_WRITE) && !d->use_dma_tx){
        if ((isr & I2C_ISR_TXIS) && d->cur_chunk){
            i2c->TXDR = d->wbuf[d->wpos++];
            d->cur_chunk--;
        }
    }

    /* RX via IRQ (quando não DMA) */
    if ((d->st == I2C_ST_READ) && !d->use_dma_rx){
        if ((isr & I2C_ISR_RXNE) && d->cur_chunk){
            d->rbuf[d->rpos++] = (uint8_t)i2c->RXDR;
            d->cur_chunk--;
        }
    }

    /* RELOAD: programar próximo chunk */
    if (isr & I2C_ISR_TCR){
        if (d->st == I2C_ST_WRITE){
            size_t rem = d->wlen - d->wpos;
            if (rem){
                size_t chunk = (rem > 255)? 255 : rem;
                d->cur_chunk = chunk;
                if (d->use_dma_tx){
                    if (!i2c_dma_start_tx(d, &d->wbuf[d->wpos], chunk)){
                        i2c_finish(d, I2C_XFER_ERR_DMA); return;
                    }
                }
                int reload  = (rem > chunk);
                int autoend = (!reload && d->rlen==0);
                i2c_prog_cr2_chunk(i2c, d->addr7, chunk, 0, autoend, reload);
            }
        } else if (d->st == I2C_ST_READ){
            size_t rem = d->rlen - d->rpos;
            if (rem){
                size_t chunk = (rem > 255)? 255 : rem;
                d->cur_chunk = chunk;
                if (d->use_dma_rx){
                    if (!i2c_dma_start_rx(d, &d->rbuf[d->rpos], chunk)){
                        i2c_finish(d, I2C_XFER_ERR_DMA); return;
                    }
                }
                int reload  = (rem > chunk);
                int autoend = (!reload);
                i2c_prog_cr2_chunk(i2c, d->addr7, chunk, 1, autoend, reload);
            }
        }
    }

    /* Fim de fase sem AUTOEND: TC */
    if (isr & I2C_ISR_TC){
        if (d->st == I2C_ST_WRITE){
            if (d->rlen){
                /* RESTART como READ */
                d->st = I2C_ST_RESTART_FOR_READ;
                size_t chunk = (d->rlen > 255)? 255 : d->rlen;
                d->cur_chunk = chunk;
                if (d->use_dma_rx){
                    if (!i2c_dma_start_rx(d, &d->rbuf[d->rpos], chunk)){
                        i2c_finish(d, I2C_XFER_ERR_DMA); return;
                    }
                }
                int reload  = (d->rlen > chunk);
                int autoend = (!reload);
                i2c_prog_cr2_chunk(i2c, d->addr7, chunk, 1, autoend, reload);
                d->st = I2C_ST_READ;
            } else {
                /* write-only → STOP manual */
                i2c_issue_stop(i2c);
            }
        }
    }

    /* STOP: fim da transação */
    if (isr & I2C_ISR_STOPF){
        i2c->ICR = I2C_ICR_STOPCF;
        i2c_finish(d, I2C_XFER_OK);
    }
}

/* ===== ISRs públicas ===== */
void I2C1_IRQHandler(void){ if (s_i2c1) i2c_isr_core(s_i2c1); }
void I2C2_IRQHandler(void){ if (s_i2c2) i2c_isr_core(s_i2c2); }
