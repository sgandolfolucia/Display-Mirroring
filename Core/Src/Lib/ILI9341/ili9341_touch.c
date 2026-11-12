// inspired by
// https://github.com/afiskon/stm32-ili9341

#include "ILI9341_STM32_Driver.h"
#include "cmsis_gcc.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_def.h"
#include "stm32u5xx_hal_spi.h"
#include <stdint.h>
#include <stdlib.h>
#include "ili9341_touch.h"
#include "bitbang.h"

#define READ_X 0xD0
#define READ_Y 0x90

bool t_pressed() {
    return HAL_GPIO_ReadPin(ILI9341_TOUCH_IRQ_GPIO_Port, ILI9341_TOUCH_IRQ_Pin) == GPIO_PIN_RESET;
}

bool t_getcoordinates(uint16_t* x, uint16_t* y) {
    static const uint8_t cmd_read_x[] = { READ_X, 0, 0 };
    static const uint8_t cmd_read_y[] = { READ_Y, 0, 0 };

    volatile uint64_t avg_x = 0;
    volatile uint64_t avg_y = 0;
    uint64_t SAMPLES = 128;
    uint64_t nsamples = 0;
    uint8_t y_raw[3];
    uint8_t x_raw[3];
    
    for(uint16_t i = 0; i < SAMPLES; i++) {
        if(!t_pressed())
            break;

        nsamples++;

        CS_LOW;
        spi_bitbang_master_tx_rx_multi((uint8_t*)cmd_read_y, y_raw, 3);
        spi_bitbang_master_tx_rx_multi((uint8_t*)cmd_read_x, x_raw, 3);
        CS_HIGH;

        avg_x += (((uint16_t)x_raw[1]) << 8) | ((uint16_t)x_raw[2]);
        avg_y += (((uint16_t)y_raw[1]) << 8) | ((uint16_t)y_raw[2]);
    }

    

    if(nsamples < SAMPLES)
        return false;

    volatile uint32_t raw_x = (avg_x / SAMPLES);
    volatile uint32_t raw_y = (avg_y / SAMPLES);

    if(raw_x < ILI9341_TOUCH_MIN_RAW_X) raw_x = ILI9341_TOUCH_MIN_RAW_X;
    if(raw_x > ILI9341_TOUCH_MAX_RAW_X) raw_x = ILI9341_TOUCH_MAX_RAW_X;

    if(raw_y < ILI9341_TOUCH_MIN_RAW_Y) raw_y = ILI9341_TOUCH_MIN_RAW_Y;
    if(raw_y > ILI9341_TOUCH_MAX_RAW_Y) raw_y = ILI9341_TOUCH_MAX_RAW_Y;

    *x = (raw_x - ILI9341_TOUCH_MIN_RAW_X) * ILI9341_TOUCH_SCALE_X / (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X);
    *y = ILI9341_TOUCH_SCALE_Y - ((raw_y - ILI9341_TOUCH_MIN_RAW_Y) * ILI9341_TOUCH_SCALE_Y / (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));

    return true;
}
