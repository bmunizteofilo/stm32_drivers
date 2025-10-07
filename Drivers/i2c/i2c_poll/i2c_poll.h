/*
 * i2c_poll.h
 *
 *  Created on: Aug 15, 2025
 *      Author: Bruno
 */

#ifndef __I2C_POLL_H__
#define __I2C_POLL_H__

#include "stm32f070xx.h"

/* =========================== API =========================== */
typedef struct {
    I2C_TypeDef *inst;
    uint32_t timingr;      /* valor para TIMINGR */
    uint8_t  analog_filter_en; /* 1=ON (ANFOFF=0), 0=OFF */
    uint8_t  digital_filter;   /* 0..15 t_I2Cclk */
    uint8_t  own7bit;          /* 0 = desabilita addr própria */
} i2c_poll_cfg_t;

void i2c_poll_enable_clock(I2C_TypeDef *i2c);
void i2c_poll_reset(I2C_TypeDef *i2c);
void i2c_poll_init(const i2c_poll_cfg_t *cfg);

/* Operações blocking (polling). addr7 = endereço 7-bit (0x00..0x7F) */
bool i2c_poll_write(I2C_TypeDef *i2c, uint8_t addr7,
                    const uint8_t *buf, size_t len, uint32_t timeout_us);

bool i2c_poll_read(I2C_TypeDef *i2c, uint8_t addr7,
                   uint8_t *buf, size_t len, uint32_t timeout_us);

/* Escreve primeiro (ex.: registrador), repete START e lê. */
bool i2c_poll_write_read(I2C_TypeDef *i2c, uint8_t addr7,
                         const uint8_t *wbuf, size_t wlen,
                         uint8_t *rbuf, size_t rlen,
                         uint32_t timeout_us);

#endif /* __I2C_POLL_H__ */
