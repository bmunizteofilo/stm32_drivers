#include "gpio.h"

/* ======= Internos ======= */
static void (*s_exti_callbacks[16])(void) = {0};

void gpio_enable_port_clock(GPIO_TypeDef *GPIOx) {
    if (GPIOx == GPIOA) RCC->AHBENR |= RCC_AHBENR_IOPAEN;
    else if (GPIOx == GPIOB) RCC->AHBENR |= RCC_AHBENR_IOPBEN;
    else if (GPIOx == GPIOC) RCC->AHBENR |= RCC_AHBENR_IOPCEN;
    else if (GPIOx == GPIOD) RCC->AHBENR |= RCC_AHBENR_IOPDEN;
    else if (GPIOx == GPIOF) RCC->AHBENR |= RCC_AHBENR_IOPFEN;
}

gpio_port_id_t gpio_port_id(GPIO_TypeDef *GPIOx) {
    if (GPIOx == GPIOA) return GPIO_PORT_A;
    if (GPIOx == GPIOB) return GPIO_PORT_B;
    if (GPIOx == GPIOC) return GPIO_PORT_C;
    if (GPIOx == GPIOD) return GPIO_PORT_D;
    if (GPIOx == GPIOF) return GPIO_PORT_F;
    /* default seguro */
    return GPIO_PORT_A;
}

void gpio_pin_init(GPIO_TypeDef *GPIOx, uint8_t pin,
                   gpio_mode_t mode,
                   gpio_otype_t otype,
                   gpio_speed_t speed,
                   gpio_pupd_t pupd)
{
    gpio_enable_port_clock(GPIOx);

    /* MODER: 2 bits por pino */
    uint32_t pos = pin * 2u;
    GPIOx->MODER   = (GPIOx->MODER   & ~(0x3u << pos)) | ((uint32_t)mode  << pos);

    /* OTYPER: 1 bit por pino */
    if (otype == GPIO_OTYPE_OPENDRAIN) GPIOx->OTYPER |=  (1u << pin);
    else                               GPIOx->OTYPER &= ~(1u << pin);

    /* OSPEEDR: 2 bits por pino */
    GPIOx->OSPEEDR = (GPIOx->OSPEEDR & ~(0x3u << pos)) | ((uint32_t)speed << pos);

    /* PUPDR: 2 bits por pino */
    GPIOx->PUPDR   = (GPIOx->PUPDR   & ~(0x3u << pos)) | ((uint32_t)pupd  << pos);
}

void gpio_pin_set_altfunc(GPIO_TypeDef *GPIOx, uint8_t pin, gpio_af_t af) {
    uint8_t reg = (pin < 8u) ? 0 : 1;
    uint8_t shift = (pin % 8u) * 4u;
    GPIOx->AFR[reg] = (GPIOx->AFR[reg] & ~(0xFu << shift)) | ((uint32_t)af << shift);
}

bool gpio_lock_pin(GPIO_TypeDef *GPIOx, uint8_t pin) {
    /* Sequência de lock: LCKK + mapa de pinos */
    uint32_t bit = (1u << pin);
    uint32_t tmp = 0;

    GPIOx->LCKR = bit | (1u << 16); // set bit pin e LCKK
    GPIOx->LCKR = bit;              // reset LCKK mantendo pin
    GPIOx->LCKR = bit | (1u << 16); // set LCKK de novo
    tmp = GPIOx->LCKR;              // leitura 1
    (void)tmp;
    tmp = GPIOx->LCKR;              // leitura 2 (lock efetivado)
    (void)tmp;

    /* Bit 16 (LCKK) reflete estado do lock */
    return (GPIOx->LCKR & (1u << 16)) != 0;
}

/* ======= EXTI / SYSCFG ======= */
void gpio_exti_enable_clock(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
}

void gpio_exti_route(GPIO_TypeDef *GPIOx, uint8_t pin) {
    gpio_exti_enable_clock();
    uint8_t idx = pin / 4u;
    uint8_t shift = (pin % 4u) * 4u;

    uint32_t portcode = (uint32_t)gpio_port_id(GPIOx);
    SYSCFG->EXTICR[idx] = (SYSCFG->EXTICR[idx] & ~(0xFu << shift)) | (portcode << shift);
}

void gpio_exti_configure_line(uint8_t pin, gpio_exti_trigger_t trigger, bool enable) {
    uint32_t mask = (1u << pin);

    /* Desabilita tudo antes de configurar (seguro) */
    EXTI->IMR  &= ~mask;
    EXTI->EMR  &= ~mask;
    EXTI->RTSR &= ~mask;
    EXTI->FTSR &= ~mask;

    if (trigger & GPIO_EXTI_TRIGGER_RISING)  EXTI->RTSR |= mask;
    if (trigger & GPIO_EXTI_TRIGGER_FALLING) EXTI->FTSR |= mask;

    if (enable) EXTI->IMR |= mask;  // unmask
    /* Evento (EMR) opcional: deixar 0 normalmente */
}

void gpio_exti_set_callback(uint8_t line, void (*cb)(void)) {
    if (line < 16) s_exti_callbacks[line] = cb;
}


/* ======= ISRs: limpam PR e chamam callback ======= */
static inline void exti_handle_line(uint8_t line) {
    uint32_t mask = (1u << line);
    if (EXTI->PR & mask) {
        EXTI->PR = mask; // write-1-to-clear
        if (s_exti_callbacks[line]) s_exti_callbacks[line]();
    }
}

void EXTI0_1_IRQHandler(void) {
    exti_handle_line(0);
    exti_handle_line(1);
}

void EXTI2_3_IRQHandler(void) {
    exti_handle_line(2);
    exti_handle_line(3);
}

void EXTI4_15_IRQHandler(void) {
    for (uint8_t l = 4; l <= 15; ++l) exti_handle_line(l);
}
