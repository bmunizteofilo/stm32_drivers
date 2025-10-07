#include "i2c_poll.h"

/* Pequenos helpers de timeout (simples, baseados em decremento).
   Passe um valor de timeout_us “generoso” do seu SysTick ou outro relógio. */
static inline bool tock(uint32_t *t){ if (*t==0) return true; (*t)--; return false; }

/* Clock do periférico */
void i2c_poll_enable_clock(I2C_TypeDef *i2c){
    if (i2c == I2C1) RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    else if (i2c == I2C2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
}
void i2c_poll_reset(I2C_TypeDef *i2c){
    /* No F0 não há bit de reset dedicado no RCC para I2C; faça soft reset via CR1.PE=0 e limpa flags */
    i2c->CR1 &= ~I2C_CR1_PE;
    i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF|I2C_ICR_ADDRCF;
}

/* Init básico: define TIMINGR/filters e liga PE */
void i2c_poll_init(const i2c_poll_cfg_t *cfg){
    I2C_TypeDef *i2c = cfg->inst;
    i2c_poll_enable_clock(i2c);

    /* Desliga periférico para configurar */
    i2c->CR1 &= ~I2C_CR1_PE;

    /* Filtros */
    uint32_t cr1 = i2c->CR1 & ~(I2C_CR1_ANFOFF | I2C_CR1_DNF_Msk);
    if (cfg->analog_filter_en) cr1 &= ~I2C_CR1_ANFOFF; /* ANFOFF=0 => filtro ON */
    else                       cr1 |=  I2C_CR1_ANFOFF; /* ANFOFF=1 => filtro OFF */
    cr1 |= ((uint32_t)(cfg->digital_filter & 0xF) << I2C_CR1_DNF_Pos);
    i2c->CR1 = cr1;

    /* Endereço próprio opcional (7-bit) */
    if (cfg->own7bit){
        i2c->OAR1 = (1u<<15) | ((uint32_t)(cfg->own7bit & 0x7F) << 1);
    } else {
        i2c->OAR1 = 0;
    }

    /* Timing */
    i2c->TIMINGR = cfg->timingr;

    /* Limpa flags e liga */
    i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF|I2C_ICR_ADDRCF;
    i2c->CR1 |= I2C_CR1_PE;
}

/* Espera flag no ISR com timeout (true=ok, false=timeout) */
static bool wait_flag_set(volatile uint32_t *reg, uint32_t mask, uint32_t *t){
    while ( ((*reg) & mask) == 0u ){
        if (tock(t)) return false;
    }
    return true;
}
static bool wait_flag_clr(volatile uint32_t *reg, uint32_t mask, uint32_t *t){
    while ( ((*reg) & mask) != 0u ){
        if (tock(t)) return false;
    }
    return true;
}

/* Inicia transferência 7-bit (até 255 bytes por fase) */
static bool i2c_start7(I2C_TypeDef *i2c, uint8_t addr7, size_t nbytes, int read, int autoend, int reload, uint32_t *tout){
    if (nbytes == 0 || nbytes > 255) return false;
    /* Espera BUSY=0 para nova transação (se preferir repeated start, pule isso) */
    if (!wait_flag_clr(&i2c->ISR, I2C_ISR_BUSY, tout)) return false;

    uint32_t cr2 = 0;
    cr2 |= I2C_CR2_SADD_7(addr7);
    if (read) cr2 |= I2C_CR2_RD_WRN;
    cr2 |= I2C_CR2_NBYTES(nbytes);
    if (autoend) cr2 |= I2C_CR2_AUTOEND;  /* STOP automático ao fim */
    if (reload)  cr2 |= I2C_CR2_RELOAD;   /* manter sessão, TCR avisará */
    cr2 |= I2C_CR2_START;

    i2c->CR2 = cr2;
    return true;
}

/* Repeated START sem checar BUSY (continuação da sessão) */
static bool i2c_restart7(I2C_TypeDef *i2c, uint8_t addr7, size_t nbytes, int read, int autoend, int reload, uint32_t *tout){
    if (nbytes == 0 || nbytes > 255) return false;

    uint32_t cr2 = i2c->CR2;
    cr2 &= ~( (0x3FFu)        /* SADD + bit0 reservado na prática */
            | (1u<<10)        /* RD_WRN */
            | (0xFFu<<16)     /* NBYTES */
            | I2C_CR2_AUTOEND
            | I2C_CR2_RELOAD
            | I2C_CR2_ADD10 );
    cr2 |= I2C_CR2_SADD_7(addr7);
    if (read) cr2 |= I2C_CR2_RD_WRN;
    cr2 |= I2C_CR2_NBYTES(nbytes);
    if (autoend) cr2 |= I2C_CR2_AUTOEND;
    if (reload)  cr2 |= I2C_CR2_RELOAD;
    cr2 |= I2C_CR2_START;

    i2c->CR2 = cr2;
    return true;
}

static inline void i2c_send_stop(I2C_TypeDef *i2c){
    i2c->CR2 |= I2C_CR2_STOP;
}

/* Trata NACK/erro comuns: limpa flags e retorna false */
static bool check_and_clear_errors(I2C_TypeDef *i2c){
    uint32_t isr = i2c->ISR;
    if (isr & I2C_ISR_NACKF){ i2c->ICR = I2C_ICR_NACKCF; return false; }
    if (isr & I2C_ISR_BERR ){ i2c->ICR = I2C_ICR_BERRCF; return false; }
    if (isr & I2C_ISR_ARLO ){ i2c->ICR = I2C_ICR_ARLOCF; return false; }
    if (isr & I2C_ISR_OVR  ){ i2c->ICR = I2C_ICR_OVRCF;  return false; }
    if (isr & I2C_ISR_TIMEOUT){ i2c->ICR = I2C_ICR_TIMOUTCF; return false; }
    return true;
}

/* ============================== Write ============================== */
bool i2c_poll_write(I2C_TypeDef *i2c, uint8_t addr7,
                    const uint8_t *buf, size_t len, uint32_t timeout_us)
{
    if (!len) return true;
    uint32_t t = timeout_us;

    /* Se >255, use RELOAD e TCR (chunking). Aqui: chunk simples. */
    size_t remaining = len;
    const uint8_t *p = buf;

    /* Começa sessão */
    size_t chunk = (remaining > 255) ? 255 : remaining;
    if (!i2c_start7(i2c, addr7, chunk, 0, (remaining==chunk), (remaining>chunk), &t)) goto fail;

    while (remaining){
        /* Espera espaço para TX (TXIS preferível; TXE também indica vazio) */
        if (!wait_flag_set(&i2c->ISR, I2C_ISR_TXIS, &t)) goto fail;
        if (!check_and_clear_errors(i2c)) goto fail;

        i2c->TXDR = *p++;
        remaining--;

        if (remaining == 0) break;

        /* Se acabaram os NBYTES do chunk, esperar TCR e reprogramar NBYTES */
        if ((i2c->ISR & I2C_ISR_TCR) && remaining){
            chunk = (remaining > 255) ? 255 : remaining;
            /* mantém sessão (RELOAD) até último chunk */
            uint32_t cr2 = i2c->CR2;
            cr2 &= ~((0xFFu<<16) | I2C_CR2_AUTOEND);
            cr2 |= I2C_CR2_NBYTES(chunk);
            if (remaining == chunk) cr2 |= I2C_CR2_AUTOEND;  /* encerra com STOP automático */
            else                    cr2 |= I2C_CR2_RELOAD;   /* mais chunks virão */
            i2c->CR2 = cr2;
        }
    }

    /* Espera TC/STOP conforme AUTOEND */
    if (!wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t)) goto fail;
    i2c->ICR = I2C_ICR_STOPCF;
    return true;

fail:
    i2c_send_stop(i2c);
    (void)wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t);
    i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF;
    return false;
}

/* ============================== Read =============================== */
bool i2c_poll_read(I2C_TypeDef *i2c, uint8_t addr7,
                   uint8_t *buf, size_t len, uint32_t timeout_us)
{
    if (!len) return true;
    uint32_t t = timeout_us;

    size_t remaining = len;
    uint8_t *p = buf;

    size_t chunk = (remaining > 255) ? 255 : remaining;
    if (!i2c_start7(i2c, addr7, chunk, 1, (remaining==chunk), (remaining>chunk), &t)) goto fail;

    while (remaining){
        if (!wait_flag_set(&i2c->ISR, I2C_ISR_RXNE, &t)) goto fail;
        if (!check_and_clear_errors(i2c)) goto fail;

        *p++ = (uint8_t)i2c->RXDR;
        remaining--;

        if (remaining == 0) break;

        if ((i2c->ISR & I2C_ISR_TCR) && remaining){
            chunk = (remaining > 255) ? 255 : remaining;
            uint32_t cr2 = i2c->CR2;
            cr2 &= ~((0xFFu<<16) | I2C_CR2_AUTOEND | I2C_CR2_RD_WRN);
            cr2 |= I2C_CR2_NBYTES(chunk) | I2C_CR2_RD_WRN;
            if (remaining == chunk) cr2 |= I2C_CR2_AUTOEND;
            else                    cr2 |= I2C_CR2_RELOAD;
            i2c->CR2 = cr2;
        }
    }

    if (!wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t)) goto fail;
    i2c->ICR = I2C_ICR_STOPCF;
    return true;

fail:
    i2c_send_stop(i2c);
    (void)wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t);
    i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF;
    return false;
}

/* =========================== Write-Read ============================= */
bool i2c_poll_write_read(I2C_TypeDef *i2c, uint8_t addr7,
                         const uint8_t *wbuf, size_t wlen,
                         uint8_t *rbuf, size_t rlen,
                         uint32_t timeout_us)
{
    if (!wlen)  return i2c_poll_read(i2c, addr7, rbuf, rlen, timeout_us);
    if (!rlen)  return i2c_poll_write(i2c, addr7, wbuf, wlen, timeout_us);

    uint32_t t = timeout_us;

    /* Fase WRITE (sem AUTOEND; vamos dar RESTART) */
    size_t wrem = wlen; const uint8_t *wp = wbuf;
    size_t chunk = (wrem > 255) ? 255 : wrem;
    /* START write com RELOAD se >255, e sem AUTOEND (queremos RESTART) */
    if (!i2c_start7(i2c, addr7, chunk, 0, /*autoend*/0, (wrem>chunk), &t)) goto fail;

    while (wrem){
        if (!wait_flag_set(&i2c->ISR, I2C_ISR_TXIS, &t)) goto fail;
        if (!check_and_clear_errors(i2c)) goto fail;
        i2c->TXDR = *wp++;
        wrem--;

        if (wrem && (i2c->ISR & I2C_ISR_TCR)){
            chunk = (wrem > 255) ? 255 : wrem;
            uint32_t cr2 = i2c->CR2;
            cr2 &= ~((0xFFu<<16) | I2C_CR2_AUTOEND);
            cr2 |= I2C_CR2_NBYTES(chunk) | I2C_CR2_RELOAD; /* continuar WRITE */
            i2c->CR2 = cr2;
        }
    }

    /* Espera TC (transfer complete) para poder RESTART */
    if (!wait_flag_set(&i2c->ISR, I2C_ISR_TC, &t)) goto fail;

    /* Fase READ com RESTART e AUTOEND */
    size_t rrem = rlen; uint8_t *rp = rbuf;
    chunk = (rrem > 255) ? 255 : rrem;
    if (!i2c_restart7(i2c, addr7, chunk, 1, (rrem==chunk), (rrem>chunk), &t)) goto fail;

    while (rrem){
        if (!wait_flag_set(&i2c->ISR, I2C_ISR_RXNE, &t)) goto fail;
        if (!check_and_clear_errors(i2c)) goto fail;
        *rp++ = (uint8_t)i2c->RXDR;
        rrem--;

        if (rrem && (i2c->ISR & I2C_ISR_TCR)){
            chunk = (rrem > 255) ? 255 : rrem;
            uint32_t cr2 = i2c->CR2;
            cr2 &= ~((0xFFu<<16) | I2C_CR2_AUTOEND | I2C_CR2_RD_WRN);
            cr2 |= I2C_CR2_NBYTES(chunk) | I2C_CR2_RD_WRN;
            if (rrem == chunk) cr2 |= I2C_CR2_AUTOEND;
            else               cr2 |= I2C_CR2_RELOAD;
            i2c->CR2 = cr2;
        }
    }

    if (!wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t)) goto fail;
    i2c->ICR = I2C_ICR_STOPCF;
    return true;

fail:
    i2c_send_stop(i2c);
    (void)wait_flag_set(&i2c->ISR, I2C_ISR_STOPF, &t);
    i2c->ICR = I2C_ICR_STOPCF|I2C_ICR_NACKCF|I2C_ICR_BERRCF|I2C_ICR_ARLOCF|I2C_ICR_OVRCF|I2C_ICR_TIMOUTCF;
    return false;
}
