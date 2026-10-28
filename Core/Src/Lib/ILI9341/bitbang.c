// inspired by
// https://community.st.com/stm32-mcus-60/how-to-make-an-efficient-bit-bang-driver-on-stm32-125568

#include "bitbang.h"
#include <stdint.h>

void rx_16bits(uint16_t *rx_data) {
    for(int i = 15; i >= 0; i--) {
        SCK_LOW;
        DELAY;
        if(MISO_SET) {
            *rx_data |= 1 << i;
        }
        DELAY;
        SCK_HIGH;
        DELAY;
    }
}

void tx_byte(uint8_t data) {
    for(int i = 0; i < 8; i++) {
        SCK_LOW;
        if (data & 0x80) {
            MOSI_HIGH;
        } else {
            MOSI_LOW;
        }
        DELAY;
        SCK_HIGH;
        DELAY;
        data <<= 1;
    }
}

uint8_t spi_bitbang_master_tx_rx_byte(uint8_t data) {
    uint8_t rx_data = 0;

    // shift bits out from highest to lowest
    for (int i = 7; i >= 0; i--) {
        SCK_LOW; // received bit is latched on falling edge. first byte is nonsense
        if (data & (1 << i)) {
            MOSI_HIGH;
        } else {
            MOSI_LOW;
        }

        DELAY;
        SCK_HIGH; // transfer bit is shifted in on rising edge
        if (MISO_SET) rx_data |= 1 << i;
        
        DELAY; // wait for sck pin to reach full voltage
    }

    return rx_data;
}

void spi_bitbang_master_tx_rx_multi(uint8_t *tx_data, uint8_t *rx_buffer, uint32_t num_bytes) {
    for (uint32_t i = 0; i < num_bytes; i++) {
        rx_buffer[i] = spi_bitbang_master_tx_rx_byte(tx_data[i]);
    }
}
