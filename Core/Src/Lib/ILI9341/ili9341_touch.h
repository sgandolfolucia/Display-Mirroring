// https://github.com/afiskon/stm32-ili9341

#ifndef __ILI9341_TOUCH_H__
#define __ILI9341_TOUCH_H__

#include <stdbool.h>

/*** Redefine if necessary ***/
#define ILI9341_TOUCH_IRQ_Pin       GPIO_PIN_9 // Arduino D5
#define ILI9341_TOUCH_IRQ_GPIO_Port GPIOF
#define ILI9341_TOUCH_CS_Pin        GPIO_PIN_1 // Arduino D2
#define ILI9341_TOUCH_CS_GPIO_Port  GPIOD

// change depending on screen orientation
#define ILI9341_TOUCH_SCALE_X 240
#define ILI9341_TOUCH_SCALE_Y 320

// to calibrate uncomment UART_Printf line in ili9341_touch.c
#define ILI9341_TOUCH_MIN_RAW_X 1500
#define ILI9341_TOUCH_MAX_RAW_X 31000
#define ILI9341_TOUCH_MIN_RAW_Y 3276
#define ILI9341_TOUCH_MAX_RAW_Y 30110

// call before initializing any SPI devices

bool t_pressed();
bool t_getcoordinates(uint16_t* x, uint16_t* y);

#endif // __ILI9341_TOUCH_H__
