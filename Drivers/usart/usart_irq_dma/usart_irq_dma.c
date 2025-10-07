#include "usart_irq_dma.h"

/* ===== Instâncias globais para ISR de USART ===== */
static usart_drv_t *g_u1 = NULL;
static usart_drv_t *g_u2 = NULL;

/* ===== Rings ===== */
static inline uint32_t rb_avail(const udrv_ring_t *rb) {
	return (rb->head - rb->tail) & (rb->size - 1u);
}
static inline uint32_t rb_free(const udrv_ring_t *rb) {
	return (rb->size - 1u) - rb_avail(rb);
}
static inline void rb_put(udrv_ring_t *rb, uint8_t b) {
	uint32_t h = rb->head, nh = (h + 1u) & (rb->size - 1u);
	if (nh != rb->tail) {
		rb->buf[h] = b;
		rb->head = nh;
	}
}
static inline uint32_t rb_get(udrv_ring_t *rb, uint8_t *dst, uint32_t n) {
	uint32_t c = 0;
	while (c < n && rb->tail != rb->head) {
		dst[c++] = rb->buf[rb->tail];
		rb->tail = (rb->tail + 1u) & (rb->size - 1u);
	}
	return c;
}

/* ===== Mapeamento DMA por USART (ajuste se seu RM diferir) ===== */
static void usart_pick_dma(usart_drv_t *u) {
	if (u->inst == USART1) {
		u->dma_tx = DMA1_Channel2;
		u->tx_ch_idx = 2;
		u->dma_rx = DMA1_Channel3;
		u->rx_ch_idx = 3;
	} else {
		u->dma_tx = DMA1_Channel4;
		u->tx_ch_idx = 4;
		u->dma_rx = DMA1_Channel5;
		u->rx_ch_idx = 5;
	}
}

/* ===== Baud ===== */
static void set_baud(USART_TypeDef *us, uint32_t pclk_hz, uint32_t baud, uint8_t over8){
  if (!over8){
    uint32_t brr = (pclk_hz + baud/2u)/baud; us->BRR = brr; us->CR1 &= ~(1u<<15);
  } else {
    uint32_t d = (100u*pclk_hz + (4u*baud))/(8u*baud);
    uint32_t mant = d/100u, frac = (d - mant*100u)*8u/100u;
    us->BRR = (mant<<4)|(frac & 0x7u); us->CR1 |= (1u<<15);
  }
}

/* ===== Core config ===== */
static void core_config(usart_drv_t *u){
  USART_TypeDef *us = u->inst;

  /* habilitar clocks */
  if (us==USART1) RCC->APB2ENR |= RCC_APB2ENR_USART1EN; else RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  us->CR1 = 0;
  us->CR2 &= ~(3u<<12);
  us->CR2 |= ((uint32_t)u->cfg.stopbits << 12);

  uint32_t cr1=0;
  if (u->cfg.wordlen==UDRV_WORDLEN_9B) cr1 |= (1u<<12);
  if (u->cfg.parity==UDRV_PARITY_EVEN) cr1 |= (1u<<10);
  else if (u->cfg.parity==UDRV_PARITY_ODD) cr1 |= (1u<<10)|(1u<<9);

  set_baud(us, u->pclk_hz, u->cfg.baud, u->cfg.oversample8?1u:0u);

  /* RE/TE */
  cr1 |= (1u<<2) | (1u<<3);

  /* RX: IRQ -> RXNEIE ; DMA -> IDLEIE */
  if (u->cfg.rx_engine == UDRV_ENGINE_DMA) cr1 |= (1u<<4); /* IDLEIE */
  else                                     cr1 |= (1u<<5); /* RXNEIE */

  us->CR1 = cr1;

  /* CR3: EIE + (DMAR/DMAT) de acordo */
  us->CR3 |= (1u<<0); /* EIE */
  if (u->cfg.rx_engine == UDRV_ENGINE_DMA) us->CR3 |= (1u<<6);
  if (u->cfg.tx_engine == UDRV_ENGINE_DMA) us->CR3 |= (1u<<7);

  us->CR1 |= (1u<<0); /* UE */

  /* NVIC USART */
  nvic_enable_irq((us==USART1)?USART1_IRQn:USART2_IRQn, u->cfg.nvic_prio_usart);
}

/* ===== RX DMA circular ===== */
static void start_rx_dma(usart_drv_t *u){
  DMA_Channel_TypeDef *ch = u->dma_rx; USART_TypeDef *us=u->inst;
  ch->CCR &= ~DMA_CCR_EN;
  ch->CPAR  = (uint32_t)&us->RDR;
  ch->CMAR  = (uint32_t)u->rx_dma_buf;
  ch->CNDTR = u->rx_dma_size;
  ch->CCR   = DMA_CCR_MINC | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8 | DMA_CCR_PL_HIGH | DMA_CCR_CIRC | DMA_CCR_TEIE;
  u->rx_dma_last = u->rx_dma_size;
  ch->CCR |= DMA_CCR_EN;
}

/* ===== TX kick (IRQ) ===== */
static inline void kick_tx_irq(usart_drv_t *u){ u->inst->CR1 |= (1u<<7); /* TXEIE */ }

/* ===== TX kick (DMA) ===== */
static void kick_tx_dma(usart_drv_t *u){
  DMA_Channel_TypeDef *ch = u->dma_tx; USART_TypeDef *us = u->inst;
  if (u->cfg.tx_engine != UDRV_ENGINE_DMA) return;
  if (ch->CCR & DMA_CCR_EN) return;

  uint32_t avail = rb_avail(&u->tx_rb);
  if (!avail) return;

  uint32_t tail = u->tx_rb.tail;
  uint32_t until_end = u->tx_rb.size - tail;
  uint32_t n = (avail < until_end)? avail : until_end;
  if (!n) return;

  ch->CCR &= ~DMA_CCR_EN;
  ch->CPAR  = (uint32_t)&us->TDR;
  ch->CMAR  = (uint32_t)&u->tx_rb.buf[tail];
  ch->CNDTR = n;
  ch->CCR   = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_8 | DMA_CCR_MSIZE_8 | DMA_CCR_PL_HIGH | DMA_CCR_TCIE | DMA_CCR_TEIE;
  u->tx_dma_len = (uint16_t)n;
  ch->CCR |= DMA_CCR_EN;
}

/* ===== Callbacks do usuário ===== */
void usart_set_callbacks(usart_drv_t *u,
                         void (*on_rx_chunk)(const uint8_t*, uint32_t),
                         void (*on_tx_done)(void),
                         void (*on_error)(uint32_t, uint32_t))
{
  u->on_rx_chunk = on_rx_chunk;
  u->on_tx_done  = on_tx_done;
  u->on_error    = on_error;
}

/* ===== dma_router callbacks ===== */
static void usart_dma_tx_cb(uint32_t flags, void *ctx){
  usart_drv_t *u = (usart_drv_t*)ctx;

  if (flags & (DMA_TEIF2 | DMA_TEIF4)) { if (u->on_error) u->on_error(0, flags); return; }

  if (flags & (DMA_TCIF2 | DMA_TCIF4)) {
    /* avança tail do ring pela rajada atual e tenta próxima */
    u->tx_rb.tail = (u->tx_rb.tail + u->tx_dma_len) & (u->tx_rb.size - 1u);
    u->tx_dma_len = 0;
    if (u->on_tx_done) u->on_tx_done();
    kick_tx_dma(u);
  }
}

static void usart_dma_rx_cb(uint32_t flags, void *ctx){
  usart_drv_t *u = (usart_drv_t*)ctx;
  if (flags & (DMA_TEIF3 | DMA_TEIF5)) { if (u->on_error) u->on_error(0, flags); }
  /* RX circular normalmente não usa TC; processamento é por IDLE em USART IRQ */
}

/* ===== API ===== */
void usart_init(usart_drv_t *u, USART_TypeDef *inst, uint32_t pclk_hz,
                const usart_drv_config_t *cfg,
                uint8_t *rx_ring_or_dma_buf, uint32_t rx_len,
                uint8_t *tx_ring, uint32_t tx_len)
{
  memset(u, 0, sizeof(*u));
  u->inst = inst; u->pclk_hz = pclk_hz; u->cfg = *cfg;

  if (cfg->rx_engine == UDRV_ENGINE_DMA) { u->rx_dma_buf = rx_ring_or_dma_buf; u->rx_dma_size = rx_len; }
  else { u->rx_rb.buf = rx_ring_or_dma_buf; u->rx_rb.size = rx_len; u->rx_rb.head=u->rx_rb.tail=0; }

  u->tx_rb.buf = tx_ring; u->tx_rb.size = tx_len; u->tx_rb.head=u->tx_rb.tail=0;

  usart_pick_dma(u);
  core_config(u);

  if (cfg->rx_engine == UDRV_ENGINE_DMA) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    start_rx_dma(u);
  }
  if (cfg->tx_engine == UDRV_ENGINE_DMA) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
  }

  /* registra callbacks no roteador (somente se usar DMA) */
  if (cfg->tx_engine == UDRV_ENGINE_DMA && u->tx_ch_idx) dma_router_attach(u->tx_ch_idx, usart_dma_tx_cb, u);
  if (cfg->rx_engine == UDRV_ENGINE_DMA && u->rx_ch_idx) dma_router_attach(u->rx_ch_idx, usart_dma_rx_cb, u);

  /* guarda ponteiros para ISR */
  if (inst==USART1) g_u1 = u; else g_u2 = u;
}

/* escreve ao ring e garante disparo */
uint32_t usart_write(usart_drv_t *u, const void *data, uint32_t len){
  const uint8_t *p=(const uint8_t*)data; uint32_t done=0;
  while (done<len){
    if (rb_free(&u->tx_rb)==0){
      if (u->cfg.tx_engine == UDRV_ENGINE_DMA) kick_tx_dma(u); else kick_tx_irq(u);
      __asm volatile("nop");
      continue;
    }
    rb_put(&u->tx_rb, p[done++]);
  }
  if (u->cfg.tx_engine == UDRV_ENGINE_DMA) kick_tx_dma(u); else kick_tx_irq(u);
  return done;
}

uint32_t usart_read(usart_drv_t *u, void *out, uint32_t maxlen){
  if (u->cfg.rx_engine == UDRV_ENGINE_DMA) return 0;
  return rb_get(&u->rx_rb, (uint8_t*)out, maxlen);
}

void usart_flush(usart_drv_t *u){
  while (rb_avail(&u->tx_rb)){ if (u->cfg.tx_engine==UDRV_ENGINE_DMA) kick_tx_dma(u); __asm volatile("nop"); }
  if (u->cfg.tx_engine==UDRV_ENGINE_DMA){ while (u->dma_tx->CCR & DMA_CCR_EN) { __asm volatile("nop"); } }
  while ((u->inst->ISR & (1u<<6))==0u) { __asm volatile("nop"); } /* TC */
}

/* ===== ISR USART ===== */
static void usart_irq_core(usart_drv_t *u){
  USART_TypeDef *us = u->inst;
  uint32_t isr = us->ISR;

  /* IDLE: RX DMA → calcula delta e repassa via callback */
  if ((isr & (1u<<4)) && u->cfg.rx_engine==UDRV_ENGINE_DMA){
    (void)us->RDR; us->ICR = (1u<<4);
    uint32_t size=u->rx_dma_size, last=u->rx_dma_last, now=u->dma_rx->CNDTR;
    uint32_t delta = (last>=now)?(last-now):(last+size-now);
    u->rx_dma_last = now;

    if (delta && u->on_rx_chunk){
      uint32_t write_idx = (size - now) % size;
      uint32_t start = (write_idx + size - delta) % size;
      if (start + delta <= size){
        u->on_rx_chunk(&u->rx_dma_buf[start], delta);
      } else {
        uint32_t first = size - start;
        u->on_rx_chunk(&u->rx_dma_buf[start], first);
        u->on_rx_chunk(&u->rx_dma_buf[0], delta - first);
      }
    }
  }

  /* RXNE: RX por IRQ para enfileirar no ring */
  if ((isr & (1u<<5)) && u->cfg.rx_engine==UDRV_ENGINE_IRQ){
    uint16_t d=(uint16_t)us->RDR;
    rb_put(&u->rx_rb, (uint8_t)(d & 0xFFu));
  }

  /* TXE: TX por IRQ para drenar ring */
  if ((isr & (1u<<7)) && (us->CR1 & (1u<<7)) && u->cfg.tx_engine==UDRV_ENGINE_IRQ){
    if (rb_avail(&u->tx_rb)){
      uint8_t b=u->tx_rb.buf[u->tx_rb.tail];
      u->tx_rb.tail=(u->tx_rb.tail+1u)&(u->tx_rb.size-1u);
      us->TDR=b;
    } else {
      us->CR1 &= ~(1u<<7); /* TXEIE off */
      us->CR1 |=  (1u<<6); /* TCIE on para notificar fim */
    }
  }

  /* TC: fim do TX (IRQ) */
  if ((isr & (1u<<6)) && (us->CR1 & (1u<<6)) && u->cfg.tx_engine==UDRV_ENGINE_IRQ){
    us->ICR = (1u<<6);
    us->CR1 &= ~(1u<<6);
    if (u->on_tx_done) u->on_tx_done();
  }

  /* Erros (ORE/NE/FE/PE) */
  if (isr & ((1u<<3)|(1u<<2)|(1u<<1)|(1u<<0))){
    (void)us->RDR; us->ICR = (1u<<3)|(1u<<2)|(1u<<1)|(1u<<0);
    if (u->on_error) u->on_error(isr, 0);
  }
}

void USART1_IRQHandler(void){ if (g_u1) usart_irq_core(g_u1); }
void USART2_IRQHandler(void){ if (g_u2) usart_irq_core(g_u2); }
