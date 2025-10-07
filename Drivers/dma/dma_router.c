#include "dma_router.h"

/* ========= tabela de callbacks 1..7 (como você já fez) ========= */
typedef struct {
	dma_router_cb_t cb;
	void *ctx;
} slot_t;
static slot_t s_slots[8]; /* indexa 1..7 */

/* ========= helpers internos ========= */
static inline DMA_Channel_TypeDef* dma_ch_ptr(uint8_t ch){
    switch (ch){
        case 1: return DMA1_Channel1;
        case 2: return DMA1_Channel2;
        case 3: return DMA1_Channel3;
        case 4: return DMA1_Channel4;
        case 5: return DMA1_Channel5;
        default: return DMA1_Channel1;
    }
}
static inline void dma_router_clear_all_flags(uint8_t ch){
    DMA1->IFCR = DMA_GIF(ch) | DMA_TCIF(ch) | DMA_HTIF(ch) | DMA_TEIF(ch);
}

/* ==== dispatcher: mantém sua ordem cb(flags, ctx) ==== */
static inline void dispatch_ch(uint8_t ch, uint32_t isr_mask_tc, uint32_t isr_mask_ht, uint32_t isr_mask_te)
{
    uint32_t isr = DMA1->ISR;
    uint32_t flags = 0;
    if (isr & isr_mask_tc) { DMA1->IFCR = isr_mask_tc; flags |= isr_mask_tc; }
    if (isr & isr_mask_ht) { DMA1->IFCR = isr_mask_ht; flags |= isr_mask_ht; }
    if (isr & isr_mask_te) { DMA1->IFCR = isr_mask_te; flags |= isr_mask_te; }
    if (flags && s_slots[ch].cb) s_slots[ch].cb(flags, s_slots[ch].ctx);
}

/* ============================================================
   API existente + melhorias
   ============================================================ */
void dma_router_init(uint8_t prio)
{
    for (int i=0;i<8;i++){ s_slots[i].cb = 0; s_slots[i].ctx = 0; }

    /* Liga clock do DMA1 */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* Habilite os grupos de IRQ que seu chip possui */
    nvic_enable_irq(DMA1_Channel1_IRQn,    prio);
    nvic_enable_irq(DMA1_Channel2_3_IRQn,  prio);
    nvic_enable_irq(DMA1_Channel4_5_IRQn,  prio);
    /* Se existir 6_7 no seu part number, habilite aqui */
}

bool dma_router_attach(uint8_t ch, dma_router_cb_t cb, void *ctx)
{
    if (ch < 1 || ch > 7) return false;
    s_slots[ch].cb  = cb;
    s_slots[ch].ctx = ctx;
    return true;
}
void dma_router_detach(uint8_t ch)
{
    if (ch < 1 || ch > 7) return;
    s_slots[ch].cb = 0; s_slots[ch].ctx = 0;
}

/* ============================================================
   NOVO: configuração de canal (start/stop/util)
   ============================================================ */
bool dma_router_start(uint8_t ch, uint32_t cpar, uint32_t cmar,
                      uint16_t count, const dma_router_chan_cfg_t *cfg)
{
    if (ch < 1 || ch > 5 || !cfg) return false;

    DMA_Channel_TypeDef *CH = dma_ch_ptr(ch);

    /* Desabilita antes de mexer */
    CH->CCR &= ~DMA_CCR_EN;

    /* Limpa flags pendentes */
    dma_router_clear_all_flags(ch);

    /* Programa endereços e tamanho */
    CH->CPAR  = cpar;
    CH->CMAR  = cmar;
    CH->CNDTR = count;

    /* Monta CCR conforme cfg */
    uint32_t ccr = 0;
    if (cfg->mem_to_periph) ccr |= DMA_CCR_DIR;
    if (cfg->circular)      ccr |= DMA_CCR_CIRC;
    if (cfg->minc)          ccr |= DMA_CCR_MINC;
    if (cfg->pinc)          ccr |= DMA_CCR_PINC;

    ccr |= ((uint32_t)(cfg->psize_bits & 3u) << DMA_CCR_PSIZE_Pos);
    ccr |= ((uint32_t)(cfg->msize_bits & 3u) << DMA_CCR_MSIZE_Pos);
    ccr |= ((uint32_t)(cfg->priority   & 3u) << DMA_CCR_PL_Pos);

    if (cfg->irq_tc) ccr |= DMA_CCR_TCIE;
    if (cfg->irq_ht) ccr |= DMA_CCR_HTIE;
    if (cfg->irq_te) ccr |= DMA_CCR_TEIE;

    CH->CCR = ccr;

    /* Habilita canal */
    CH->CCR |= DMA_CCR_EN;
    return true;
}

void dma_router_stop(uint8_t ch)
{
    if (ch < 1 || ch > 5) return;
    DMA_Channel_TypeDef *CH = dma_ch_ptr(ch);
    CH->CCR &= ~DMA_CCR_EN;
    dma_router_clear_all_flags(ch);
}

void dma_router_set_length(uint8_t ch, uint16_t count)
{
    if (ch < 1 || ch > 5) return;
    dma_ch_ptr(ch)->CNDTR = count;
}

uint16_t dma_router_get_remaining(uint8_t ch)
{
    if (ch < 1 || ch > 5) return 0;
    return (uint16_t)dma_ch_ptr(ch)->CNDTR;
}

/* ============================ ISRs ============================ */
void DMA1_CH1_IRQHandler(void)
{
    dispatch_ch(1, DMA_TCIF1, DMA_HTIF1, DMA_TEIF1);
}
void DMA1_Channel2_3_IRQHandler(void)
{
    dispatch_ch(2, DMA_TCIF2, DMA_HTIF2, DMA_TEIF2);
    dispatch_ch(3, DMA_TCIF3, DMA_HTIF3, DMA_TEIF3);
}
void DMA1_Channel4_5_IRQHandler(void)
{
    dispatch_ch(4, DMA_TCIF4, DMA_HTIF4, DMA_TEIF4);
    dispatch_ch(5, DMA_TCIF5, DMA_HTIF5, DMA_TEIF5);
}
/* Se seu MCU tiver 6/7 compartilhados, acrescente aqui:
void DMA1_Channel6_7_IRQHandler(void) { dispatch_ch(6,...); dispatch_ch(7,...); }
*/

