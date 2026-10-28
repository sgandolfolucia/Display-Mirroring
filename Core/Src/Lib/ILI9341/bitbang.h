// inspired by
// https://community.st.com/stm32-mcus-60/how-to-make-an-efficient-bit-bang-driver-on-stm32-125568

#include <stdint.h>
#include "stm32u575xx.h"

/*
 * Configuration Defines
 */
#define DELAY_CYCLES 2


/*
 * Hardware Definitions for Pin Setting and Resetting
 *
 * This driver does not initialize the pins.
 */

#define CS_LOW GPIOD->BSRR = GPIO_BSRR_BR1
#define CS_HIGH GPIOD->BSRR = GPIO_BSRR_BS1

#define MOSI_LOW GPIOG->BSRR = GPIO_BSRR_BR1
#define MOSI_HIGH GPIOG->BSRR = GPIO_BSRR_BS1

#define SCK_LOW GPIOG->BSRR = GPIO_BSRR_BR8
#define SCK_HIGH GPIOG->BSRR = GPIO_BSRR_BS8

//Returns non-zero if MISO is set, 0 if non set
#define MISO_SET (GPIOG->IDR & GPIO_IDR_ID0)

#define DELAY for (uint32_t i = 0; i < DELAY_CYCLES; i++) __NOP()

uint8_t spi_bitbang_master_tx_rx_byte(uint8_t data);
void spi_bitbang_master_tx_rx_multi(uint8_t *tx_data, uint8_t *rx_buffer, uint32_t num_bytes);
void rx_16bits(uint16_t*);
void tx_byte(uint8_t);