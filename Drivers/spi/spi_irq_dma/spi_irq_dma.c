#include "spi_irq_dma.h"

static void spi_dma_start(spi_drv_t *s);
static void spi_irq_enable(spi_drv_t *s);

/* ===== Instâncias globais p/ SPI IRQ dispatch ===== */
static spi_drv_t *g_spi1 = NULL;
static spi_drv_t *g_spi2 = NULL;

/* ===== Utilidades ===== */
static inline void wait_bsy_clear(SPI_TypeDef *spi){
  while (spi->SR & (1u<<7)) { __asm volatile("nop"); }  /* BSY */
}

/* Mapeamento DMA por instância (ajuste se seu part number diferir) */
static void spi_pick_dma(spi_drv_t *s){
  if (s->inst == SPI1){
    s->dma_rx = DMA1_Channel2; s->rx_ch_idx = 2;
    s->dma_tx = DMA1_Channel3; s->tx_ch_idx = 3;
  } else {
    s->dma_rx = DMA1_Channel4; s->rx_ch_idx = 4;
    s->dma_tx = DMA1_Channel5; s->tx_ch_idx = 5;
  }
}

/* inicia uma fase (assert_cs controla se chamamos cs_assert agora) */
static bool spi_start_phase(spi_drv_t *s, const void *tx, void *rx, uint32_t count, bool assert_cs)
{
  s->tx_buf = tx; s->rx_buf = rx; s->count = count;
  s->tx_idx = 0;  s->rx_idx = 0;
  s->dma_tx_done = s->dma_rx_done = 0;

  if (assert_cs && s->cfg.nss_mode == SPI_NSS_SOFT && s->cfg.cs_assert) s->cfg.cs_assert();

  /* limpa OVR prévio */
  (void)s->inst->SR; (void)s->inst->DR;

  if (s->cfg.tx_engine == SPI_ENGINE_DMA || s->cfg.rx_engine == SPI_ENGINE_DMA) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    spi_dma_start(s);   /* usa buffers já colocados em s */
  } else {
    spi_irq_enable(s);
  }
  return true;
}


/* Finalização comum */
static void spi_finish(spi_drv_t *s){
  wait_bsy_clear(s->inst);

  /* Se estamos em transação multi-fase e acabamos a FASE 1:
     -> NÃO libera CS, NÃO limpa busy; dispara fase 2 sem assert de CS */
  if (s->multi_active && s->phase == 1) {
    s->phase = 2;

    /* Fase 2: TX = phase2_tx (normalmente NULL → dummy), RX = phase2_rx */
    /* Aqui NÃO assertamos CS (mantém baixo do começo da fase 1) */
    spi_start_phase(s, s->phase2_tx, s->phase2_rx, s->phase2_count, /*assert_cs=*/false);
    return;
  }

  /* Fim normal (fase única ou fim da fase 2) */
  if (s->cfg.nss_mode == SPI_NSS_SOFT && s->cfg.cs_release) s->cfg.cs_release();

  s->busy = 0;

  /* callback do usuário:
     - em fluxo multi-fase, chamamos somente ao final da fase 2 */
  if (s->multi_active) {
    s->multi_active = 0;
    if (s->user_on_complete) s->user_on_complete();
  } else if (s->on_complete) {
    s->on_complete();
  }
}


/* ===== Callbacks do dma_router (um por direção) ===== */
static void spi_dma_tx_cb(uint32_t flags, void *ctx){
  spi_drv_t *s = (spi_drv_t*)ctx;
  if (!s->tx_dma_active) return;

  if (flags & (DMA_TEIF3 | DMA_TEIF5)) {  /* erro TX nos canais típicos */
    if (s->on_error) s->on_error(0, flags);
    s->tx_dma_active = 0;
    return;
  }
  if (flags & (DMA_TCIF3 | DMA_TCIF5)) {  /* TX done */
    s->dma_tx_done = 1;
    s->tx_dma_active = 0;
    /* se RX não usa DMA ou já terminou, finaliza */
    if (!s->rx_dma_active || s->dma_rx_done) spi_finish(s);
  }
}

static void spi_dma_rx_cb(uint32_t flags, void *ctx){
  spi_drv_t *s = (spi_drv_t*)ctx;
  if (!s->rx_dma_active) return;

  if (flags & (DMA_TEIF2 | DMA_TEIF4)) {  /* erro RX nos canais típicos */
    if (s->on_error) s->on_error(0, flags);
    s->rx_dma_active = 0;
    return;
  }
  if (flags & (DMA_TCIF2 | DMA_TCIF4)) {  /* RX done */
    s->dma_rx_done = 1;
    s->rx_dma_active = 0;
    if (!s->tx_dma_active || s->dma_tx_done) spi_finish(s);
  }
}

/* ===== Inicialização ===== */
void spi_init(spi_drv_t *s, SPI_TypeDef *inst, const spi_drv_config_t *cfg)
{
  memset(s, 0, sizeof(*s));
  s->inst = inst;
  s->cfg  = *cfg;
  s->bytes_per_item = (cfg->datasize <= 8) ? 1u : 2u;
  s->tx_dummy8 = 0xFF; s->tx_dummy16 = 0xFFFF;

  /* clocks */
  if (inst == SPI1) RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
  else              RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

  /* mapear DMA (e habilitar clock do DMA se qualquer engine usar DMA) */
  spi_pick_dma(s);
  if (cfg->tx_engine == SPI_ENGINE_DMA || cfg->rx_engine == SPI_ENGINE_DMA) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
  }

  /* configurar SPI */
  inst->CR1 &= ~(1u<<6); /* SPE=0 */

  uint32_t cr1 = 0;
  if (cfg->mode & 0x2) cr1 |= (1u<<1); /* CPOL */
  if (cfg->mode & 0x1) cr1 |= (1u<<0); /* CPHA */
  cr1 |= (1u<<2);                      /* MSTR */
  cr1 |= ((uint32_t)cfg->baud_div & 0x7u) << 3; /* BR */
  if (cfg->bit_order == SPI_LSB_FIRST) cr1 |= (1u<<7);

  if (cfg->nss_mode == SPI_NSS_SOFT) cr1 |= (1u<<9)|(1u<<8); /* SSM|SSI */
  else                               cr1 &= ~((1u<<9)|(1u<<8));

  uint32_t cr2 = 0;
  uint32_t ds = (cfg->datasize >= 4 && cfg->datasize <= 16) ? (cfg->datasize - 1u) : 7u;
  cr2 |= (ds & 0xF) << 8;                 /* DS */
  if (cfg->datasize <= 8) cr2 |= (1u<<12);/* FRXTH */
  if (cfg->nss_mode == SPI_NSS_HARD_AUTO) {
    cr2 |= (1u<<2);                       /* SSOE */
    if (cfg->nssp_pulse) cr2 |= (1u<<3);  /* NSSP (se disponível) */
  }

  inst->CR2 = cr2;
  inst->CR1 = cr1;
  inst->CR1 |= (1u<<6); /* SPE=1 */

  /* NVIC SPI */
  nvic_enable_irq((inst==SPI1)?SPI1_IRQn:SPI2_IRQn, cfg->nvic_prio_spi);

  /* registra callbacks no roteador (uma vez por vida do handle) */
  if (s->rx_ch_idx) dma_router_attach(s->rx_ch_idx, spi_dma_rx_cb, s);
  if (s->tx_ch_idx) dma_router_attach(s->tx_ch_idx, spi_dma_tx_cb, s);

  /* registra p/ as ISRs do SPI */
  if (inst==SPI1) g_spi1 = s; else g_spi2 = s;
}

/* ===== Callbacks do usuário ===== */
void spi_set_callbacks(spi_drv_t *s, void (*on_complete)(void),
                                  void (*on_error)(uint32_t,uint32_t))
{
  s->on_complete = on_complete;
  s->on_error    = on_error;
  /* também guardamos as versões “do usuário” para multi-fase */
  s->user_on_complete = on_complete;
  s->user_on_error    = on_error;
}


/* ===== Abort ===== */
void spi_abort(spi_drv_t *s)
{
  SPI_TypeDef *spi = s->inst;

  /* desliga IRQs do SPI (TXEIE/RXNEIE ficam em CR2) */
  spi->CR2 &= ~((1u<<7)|(1u<<6));

  /* desliga DMAs se ativos */
  if (s->dma_tx) s->dma_tx->CCR &= ~DMA_CCR_EN;
  if (s->dma_rx) s->dma_rx->CCR &= ~DMA_CCR_EN;
  s->tx_dma_active = s->rx_dma_active = 0;

  /* limpa OVR e estados */
  (void)spi->SR; (void)spi->DR;

  wait_bsy_clear(spi);
  if (s->cfg.nss_mode == SPI_NSS_SOFT && s->cfg.cs_release) s->cfg.cs_release();

  s->busy = 0;
  s->multi_active = 0;
  s->phase = 0;
}

/* ===== Wait ===== */
void spi_wait(spi_drv_t *s){ while (s->busy) { __asm volatile("nop"); } }

/* ===== Caminho IRQ ===== */
static void spi_irq_enable(spi_drv_t *s){
  uint32_t cr2 = s->inst->CR2;
  /* geramos clock mesmo em RX-only (TXEIE liga) e evitamos OVR mesmo em TX-only (RXNEIE liga) */
  cr2 |= (1u<<6) | (1u<<7); /* RXNEIE | TXEIE */
  s->inst->CR2 = cr2;
}
static void spi_irq_disable(spi_drv_t *s){ s->inst->CR2 &= ~((1u<<7)|(1u<<6)); }

static void spi_irq_handler(spi_drv_t *s){
  SPI_TypeDef *spi = s->inst;
  uint32_t sr = spi->SR;

  /* RXNE */
  if ((sr & (1u<<0)) && (spi->CR2 & (1u<<6))) {
    if (s->bytes_per_item == 1) {
      uint8_t d = *(volatile uint8_t*)&spi->DR;
      if (s->rx_buf && s->rx_idx < s->count) ((uint8_t*)s->rx_buf)[s->rx_idx] = d;
    } else {
      uint16_t d = *(volatile uint16_t*)&spi->DR;
      if (s->rx_buf && s->rx_idx < s->count) ((uint16_t*)s->rx_buf)[s->rx_idx] = d;
    }
    if (s->rx_idx < s->count) s->rx_idx++;
  }

  /* TXE */
  if ((sr & (1u<<1)) && (spi->CR2 & (1u<<7))) {
    if (s->tx_idx < s->count) {
      if (s->bytes_per_item == 1) {
        uint8_t out = s->tx_buf ? ((const uint8_t*)s->tx_buf)[s->tx_idx] : 0xFFu;
        *(volatile uint8_t*)&spi->DR = out;
      } else {
        uint16_t out = s->tx_buf ? ((const uint16_t*)s->tx_buf)[s->tx_idx] : 0xFFFFu;
        *(volatile uint16_t*)&spi->DR = out;
      }
      s->tx_idx++;
    } else {
      /* tudo transmitido → podemos desligar TXEIE (RXNEIE fica ligado até colher todo RX) */
      spi->CR2 &= ~(1u<<7);
    }
  }

  /* erros (MODF/OVR/CRCERR) */
  if (sr & ((1u<<6)|(1u<<5)|(1u<<4))) {
    (void)spi->DR; (void)spi->SR; /* limpa OVR */
    if (s->on_error) s->on_error(sr, 0);
  }

  /* terminou? (todos itens TX e RX processados) */
  if (s->tx_idx >= s->count && s->rx_idx >= s->count) {
    spi_irq_disable(s);
    spi_finish(s);
  }
}

/* ===== Caminho DMA ===== */
static void spi_dma_start(spi_drv_t *s)
{
  SPI_TypeDef *spi = s->inst;

  /* Habilita bits de DMA na SPI */
  uint32_t cr2 = spi->CR2;
  /* Vamos decidir dinamicamente se ativaremos RX/TX DMA nessa transação */
  s->rx_dma_active = 0; s->tx_dma_active = 0;

  /* RX: se o engine é DMA OU se precisamos drenar RX (TX-only por DMA) */
  bool need_rx_dma = (s->cfg.rx_engine == SPI_ENGINE_DMA) ||
                     ((s->cfg.tx_engine == SPI_ENGINE_DMA) && (s->rx_buf == NULL));
  if (need_rx_dma) {
    DMA_Channel_TypeDef *ch = s->dma_rx;
    ch->CCR &= ~DMA_CCR_EN;
    ch->CPAR  = (uint32_t)&spi->DR;
    ch->CNDTR = s->count;
    if (s->bytes_per_item == 1) {
      ch->CMAR = (uint32_t)(s->rx_buf ? s->rx_buf : &s->rx_discard8);
      ch->CCR  = (s->rx_buf ? DMA_CCR_MINC : 0) | DMA_CCR_PL_HIGH | DMA_CCR_TCIE | DMA_CCR_TEIE;
    } else {
      ch->CMAR = (uint32_t)(s->rx_buf ? s->rx_buf : &s->rx_discard16);
      ch->CCR  = (s->rx_buf ? DMA_CCR_MINC : 0) | DMA_CCR_PL_HIGH | DMA_CCR_TCIE | DMA_CCR_TEIE
               | DMA_CCR_PSIZE_16 | DMA_CCR_MSIZE_16;
    }
    ch->CCR |= DMA_CCR_EN;
    s->dma_rx_done = 0;
    s->rx_dma_active = 1;
    cr2 |= (1u<<0); /* RXDMAEN */
  }

  /* TX: se o engine é DMA OU se precisamos gerar clock (RX-only por DMA) */
  bool need_tx_dma = (s->cfg.tx_engine == SPI_ENGINE_DMA) ||
                     ((s->cfg.rx_engine == SPI_ENGINE_DMA) && (s->tx_buf == NULL));
  if (need_tx_dma) {
    DMA_Channel_TypeDef *ch = s->dma_tx;
    ch->CCR &= ~DMA_CCR_EN;
    ch->CPAR  = (uint32_t)&spi->DR;
    ch->CNDTR = s->count;
    if (s->bytes_per_item == 1) {
      ch->CMAR = (uint32_t)(s->tx_buf ? s->tx_buf : &s->tx_dummy8);
      ch->CCR  = DMA_CCR_DIR | (s->tx_buf ? DMA_CCR_MINC : 0) | DMA_CCR_PL_HIGH | DMA_CCR_TCIE | DMA_CCR_TEIE;
    } else {
      ch->CMAR = (uint32_t)(s->tx_buf ? s->tx_buf : &s->tx_dummy16);
      ch->CCR  = DMA_CCR_DIR | (s->tx_buf ? DMA_CCR_MINC : 0) | DMA_CCR_PL_HIGH | DMA_CCR_TCIE | DMA_CCR_TEIE
               | DMA_CCR_PSIZE_16 | DMA_CCR_MSIZE_16;
    }
    ch->CCR |= DMA_CCR_EN;
    s->dma_tx_done = 0;
    s->tx_dma_active = 1;
    cr2 |= (1u<<1); /* TXDMAEN */
  }

  spi->CR2 = cr2;
}

/* ===== API principal ===== */
bool spi_transfer_async(spi_drv_t *s, const void *tx, void *rx, uint32_t count)
{
  if (s->busy) return false;
  s->busy  = 1;
  s->multi_active = 0;   /* transação normal (fase única) */
  s->phase = 1;

  /* Fase única com assert de CS se NSS_SOFT */
  return spi_start_phase(s, tx, rx, count, /*assert_cs=*/true);
}

bool spi_write_then_read_async(spi_drv_t *s,
                               const void *tx1, uint32_t n_tx1,
                               void *rx2, uint32_t n_rx2)
{
  if (s->busy) return false;

  /* Requer NSS_SOFT para manter CS baixo entre as fases */
  if (s->cfg.nss_mode != SPI_NSS_SOFT) {
    /* fallback: pode-se emular com duas chamadas, mas CS vai “pulsar”.
       Aqui retornamos false para deixar o uso explícito. */
    return false;
  }

  s->busy = 1;
  s->multi_active = 1;
  s->phase = 1;

  /* preparar fase 2 */
  s->phase2_tx    = NULL;     /* dummy clock na segunda fase */
  s->phase2_rx    = rx2;
  s->phase2_count = n_rx2;

  /* fase 1: envia comando (RX descartado) com CS assertado */
  return spi_start_phase(s, tx1, /*rx=*/NULL, n_tx1, /*assert_cs=*/true);
}



/* ===== ISRs de SPI ===== */
static void spi_isr_common(spi_drv_t *s){
  if (!s || !s->busy) return;
  /* caminho IRQ: tratamos fluxo; em DMA apenas checamos erros */
  if (s->cfg.tx_engine == SPI_ENGINE_IRQ || s->cfg.rx_engine == SPI_ENGINE_IRQ) {
    spi_irq_handler(s);
  } else {
    uint32_t sr = s->inst->SR;
    if (sr & ((1u<<6)|(1u<<5)|(1u<<4))) {
      (void)s->inst->DR; (void)s->inst->SR;
      if (s->on_error) s->on_error(sr, 0);
    }
  }
}

void SPI1_IRQHandler(void){ spi_isr_common(g_spi1); }
void SPI2_IRQHandler(void){ spi_isr_common(g_spi2); }
