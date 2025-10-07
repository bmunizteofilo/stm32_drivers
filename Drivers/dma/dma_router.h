/*
 * dma_router.h
 *
 *  Created on: Aug 14, 2025
 *      Author: Bruno
 */

#ifndef __DMA_ROUTER_H__
#define __DMA_ROUTER_H__

#include "stm32f070xx.h"

/* Tipo de callback por canal: flags = bitmap de TC/HT/TE já FILTRADOS daquele canal */
typedef void (*dma_router_cb_t)(uint32_t flags, void *ctx);

/* API */
void dma_router_init(uint8_t nvic_priority_0_to_3);
bool dma_router_attach(uint8_t ch_index_1_to_7, dma_router_cb_t cb, void *ctx);
void dma_router_detach(uint8_t ch_index_1_to_7);

/* NOVO: configuração “one-shot” de um canal (1..5) e controle */
typedef struct {
    uint8_t mem_to_periph;   /* 1: mem->periph (CCR.DIR) */
    uint8_t circular;        /* CCR.CIRC */
    uint8_t minc;            /* CCR.MINC */
    uint8_t pinc;            /* CCR.PINC */
    uint8_t msize_bits;      /* 0:8, 1:16, 2:32 (CCR.MSIZE) */
    uint8_t psize_bits;      /* 0:8, 1:16, 2:32 (CCR.PSIZE) */
    uint8_t priority;        /* 0..3 (CCR.PL) */
    uint8_t irq_tc;          /* CCR.TCIE */
    uint8_t irq_ht;          /* CCR.HTIE */
    uint8_t irq_te;          /* CCR.TEIE */
} dma_router_chan_cfg_t;

bool     dma_router_start(uint8_t ch, uint32_t cpar, uint32_t cmar,
                          uint16_t count, const dma_router_chan_cfg_t *cfg);
void     dma_router_stop(uint8_t ch);
void     dma_router_set_length(uint8_t ch, uint16_t count);
uint16_t dma_router_get_remaining(uint8_t ch);

/* Wrappers comuns */
static inline bool dma_router_start_mem2periph_16(uint8_t ch,
        volatile void *periph_reg, const void *mem, uint16_t len,
        bool circular, uint8_t priority, bool tc_irq, bool te_irq)
{
    dma_router_chan_cfg_t cfg = {
        .mem_to_periph = 1, .circular = circular, .minc = 1, .pinc = 0,
        .msize_bits = 1, .psize_bits = 1, .priority = (priority & 3),
        .irq_tc = tc_irq, .irq_ht = 0, .irq_te = te_irq
    };
    return dma_router_start(ch, (uint32_t)periph_reg, (uint32_t)mem, len, &cfg);
}

static inline bool dma_router_start_periph2mem_16(uint8_t ch,
        volatile void *periph_reg, void *mem, uint16_t len,
        bool circular, uint8_t priority, bool tc_irq, bool te_irq)
{
    dma_router_chan_cfg_t cfg = {
        .mem_to_periph = 0, .circular = circular, .minc = 1, .pinc = 0,
        .msize_bits = 1, .psize_bits = 1, .priority = (priority & 3),
        .irq_tc = tc_irq, .irq_ht = 0, .irq_te = te_irq
    };
    return dma_router_start(ch, (uint32_t)periph_reg, (uint32_t)mem, len, &cfg);
}

#endif /* __DMA_ROUTER_H__ */
