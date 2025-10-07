#include "spi_poll.h"

/* ===== Esperas com timeout em ciclos ===== */
static inline bool wait_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t t){
  while (t--) { if ((*reg & mask) != 0u) return true; __asm volatile("nop"); }
  return false;
}
static inline bool wait_flag_clr(volatile uint32_t *reg, uint32_t mask, uint32_t t){
  while (t--) { if ((*reg & mask) == 0u) return true; __asm volatile("nop"); }
  return false;
}

/* ===== Clocks ===== */
static void spi_enable_clock(SPI_TypeDef *inst){
  if (inst == SPI1) RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
  else              RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
}

/* ===== Config core ===== */
void spi_poll_init(spi_poll_t *s, SPI_TypeDef *inst,
		const spi_poll_config_t *cfg) {
	s->inst = inst;
	s->cfg = *cfg;

	spi_enable_clock(inst);

	/* Desliga SPI antes de configurar */
	inst->CR1 &= ~(1u << 6); /* SPE=0 */
	spi_poll_clear_errors(s);

	/* ---------- CR1 ---------- */
	uint32_t cr1 = 0;
	if (cfg->mode & 0x2)
		cr1 |= (1u << 1); /* CPOL */
	if (cfg->mode & 0x1)
		cr1 |= (1u << 0); /* CPHA */
	cr1 |= (1u << 2); /* MSTR */
	cr1 |= ((uint32_t) cfg->baud_div & 0x7u) << 3; /* BR */
	if (cfg->bit_order == SPI_LSB_FIRST)
		cr1 |= (1u << 7);

	/* NSS via software (SSM/SSI) ou via hardware (SSOE) */
	if (cfg->nss_mode == SPI_NSS_SOFT) {
		cr1 |= (1u << 9) | (1u << 8); /* SSM=1, SSI=1 (NSS interno alto) */
	} else {
		/* SSM=0 (por padrão). SSOE será habilitado em CR2. */
		cr1 &= ~((1u << 9) | (1u << 8));
	}

	/* ---------- CR2 ---------- */
	uint32_t cr2 = 0;
	uint32_t ds =
			(cfg->datasize >= 4 && cfg->datasize <= 16) ?
					(cfg->datasize - 1u) : 7u;
	cr2 |= (ds & 0xF) << 8;
	if (cfg->datasize <= 8)
		cr2 |= (1u << 12); /* FRXTH=1 para RXNE em 8-bit */

	if (cfg->nss_mode == SPI_NSS_HARD_AUTO) {
		cr2 |= (1u << 2); /* SSOE: NSS por hardware em modo master */
		if (cfg->nssp_pulse) {
			/* Alguns F0 têm NSSP (pulso de NSS) em CR2 bit3 — use somente se seu part number suportar */
			cr2 |= (1u << 3); /* NSSP */
		}
	}

	inst->CR2 = cr2;
	inst->CR1 = cr1;

	/* Liga SPI */
	inst->CR1 |= (1u << 6); /* SPE=1 */
}

/* ===== Limpar erros e esvaziar FIFO ===== */
void spi_poll_clear_errors(spi_poll_t *s)
{
  SPI_TypeDef *spi = s->inst;
  /* Leia SR/DR para limpar OVR, zere CRCERR, MODF é limpo ao escrever CR1 */
  (void)spi->SR;
  (void)spi->DR;
  spi->CR1 &= ~(1u<<6); /* SPE=0 para limpar MODF se houver */
  spi->CR1 |=  (1u<<6); /* religa */
  /* CRCERR (se usado) limpa escrevendo 0 em SR.CRCERR, mas no F0 é via SR? — ignoramos CRC no polling simples */
}

/* ===== Transferências ===== */
static inline uint16_t dummy_word(spi_datasize_t ds){ return (ds <= 8) ? 0xFFu : 0xFFFFu; }

/* NOTA (F0):
   - DR aceita escritas/leitura de 8 ou 16 bits dependendo de DS.
   - Para 8 bits, acesse DR como byte; para 16, como halfword. */
uint32_t spi_poll_transfer(spi_poll_t *s, const void *tx, void *rx,
		uint32_t count, uint32_t tmo_cycles_per_item) {

	SPI_TypeDef *spi = s->inst;
	const uint8_t *tx8 = (const uint8_t*) tx;
	const uint16_t *tx16 = (const uint16_t*) tx;
	uint8_t *rx8 = (uint8_t*) rx;
	uint16_t *rx16 = (uint16_t*) rx;
	uint32_t done = 0;

	/* Assert CS só no modo SOFT */
	if (s->cfg.nss_mode == SPI_NSS_SOFT && s->cfg.cs_assert)
		s->cfg.cs_assert();

	if (s->cfg.datasize <= 8) {
		while (done < count) {
			if (!wait_flag_set(&spi->SR, (1u << 1)/*TXE*/, tmo_cycles_per_item))
				break;
			uint8_t dout = tx ? tx8[done] : 0xFFu;
			*(volatile uint8_t*) &spi->DR = dout;

			if (!wait_flag_set(&spi->SR, (1u << 0)/*RXNE*/,
					tmo_cycles_per_item))
				break;
			uint8_t din = *(volatile uint8_t*) &spi->DR;
			if (rx)
				rx8[done] = din;
			done++;
		}
	} else {
		while (done < count) {
			if (!wait_flag_set(&spi->SR, (1u << 1), tmo_cycles_per_item))
				break;
			uint16_t dout = tx ? tx16[done] : 0xFFFFu;
			*(volatile uint16_t*) &spi->DR = dout;

			if (!wait_flag_set(&spi->SR, (1u << 0), tmo_cycles_per_item))
				break;
			uint16_t din = *(volatile uint16_t*) &spi->DR;
			if (rx)
				rx16[done] = din;
			done++;
		}
	}

	(void) wait_flag_clr(&spi->SR, (1u << 7)/*BSY*/, tmo_cycles_per_item);

	if (s->cfg.nss_mode == SPI_NSS_SOFT && s->cfg.cs_release)
		s->cfg.cs_release();
	return done;
}

uint32_t spi_poll_write(spi_poll_t *s, const void *tx, uint32_t count, uint32_t tmo)
{
  return spi_poll_transfer(s, tx, NULL, count, tmo);
}
uint32_t spi_poll_read(spi_poll_t *s, void *rx, uint32_t count, uint32_t tmo)
{
  return spi_poll_transfer(s, NULL, rx, count, tmo);
}
