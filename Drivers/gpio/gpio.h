#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>
#include <stdbool.h>

#include "stm32f070xx.h"


/* ================== DEFINIÇÕES DE CONFIG ================== */
typedef enum {
    GPIO_MODE_INPUT  = 0x0,
    GPIO_MODE_OUTPUT = 0x1,
    GPIO_MODE_ALT    = 0x2,
    GPIO_MODE_ANALOG = 0x3
} gpio_mode_t;

typedef enum {
    GPIO_OTYPE_PUSHPULL  = 0x0,
    GPIO_OTYPE_OPENDRAIN = 0x1
} gpio_otype_t;

typedef enum {
    GPIO_PUPD_NONE = 0x0,
    GPIO_PUPD_UP   = 0x1,
    GPIO_PUPD_DOWN = 0x2
} gpio_pupd_t;

typedef enum {
    GPIO_SPEED_LOW    = 0x0, // ~2 MHz
    GPIO_SPEED_MEDIUM = 0x1, // ~10 MHz
    GPIO_SPEED_HIGH   = 0x3  // ~50 MHz (no F0, 0b10 reservado)
} gpio_speed_t;

typedef enum {
    GPIO_AF0 = 0,
    GPIO_AF1 = 1,
    GPIO_AF2 = 2,
    GPIO_AF3 = 3,
    GPIO_AF4 = 4,
    GPIO_AF5 = 5,
    GPIO_AF6 = 6,
    GPIO_AF7 = 7
} gpio_af_t;

typedef enum {
    GPIO_EXTI_TRIGGER_RISING  = 1,
    GPIO_EXTI_TRIGGER_FALLING = 2,
    GPIO_EXTI_TRIGGER_BOTH    = 3
} gpio_exti_trigger_t;

/* Handle “lógico” de porta para SYSCFG EXTICR */
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_D = 3,
    GPIO_PORT_F = 5, // nos EXTICR, F é 0b101
} gpio_port_id_t;

/* ================== API ================== */
void gpio_enable_port_clock(GPIO_TypeDef *GPIOx);
gpio_port_id_t gpio_port_id(GPIO_TypeDef *GPIOx);

void gpio_pin_init(GPIO_TypeDef *GPIOx, uint8_t pin,
                   gpio_mode_t mode,
                   gpio_otype_t otype,
                   gpio_speed_t speed,
                   gpio_pupd_t pupd);

void gpio_pin_set_altfunc(GPIO_TypeDef *GPIOx, uint8_t pin, gpio_af_t af);

static inline void gpio_write_pin(GPIO_TypeDef *GPIOx, uint8_t pin, bool state) {
    if (state) GPIOx->BSRR = (1u << pin);
    else       GPIOx->BRR  = (1u << pin);
}
static inline void gpio_toggle_pin(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->ODR ^= (1u << pin);
}
static inline bool gpio_read_pin(GPIO_TypeDef *GPIOx, uint8_t pin) {
    return (GPIOx->IDR >> pin) & 1u;
}
static inline void gpio_write_port(GPIO_TypeDef *GPIOx, uint16_t mask, uint16_t value) {
    GPIOx->ODR = (GPIOx->ODR & ~mask) | (value & mask);
}
static inline uint16_t gpio_read_port(GPIO_TypeDef *GPIOx) {
    return (uint16_t)GPIOx->IDR;
}

bool gpio_lock_pin(GPIO_TypeDef *GPIOx, uint8_t pin);

/* ========== EXTI / SYSCFG ========== */
void gpio_exti_enable_clock(void);
void gpio_exti_route(GPIO_TypeDef *GPIOx, uint8_t pin);
void gpio_exti_configure_line(uint8_t pin, gpio_exti_trigger_t trigger, bool enable);
void gpio_exti_set_callback(uint8_t line, void (*cb)(void));

/* ISRs fracas que chamam callbacks */
void EXTI0_1_IRQHandler(void);
void EXTI2_3_IRQHandler(void);
void EXTI4_15_IRQHandler(void);

#endif /* __GPIO_H__ */
