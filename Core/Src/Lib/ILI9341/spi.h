// exposes spi functionality to both the display driver
// and the main application
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_spi.h"

extern SPI_HandleTypeDef hspi2;

void MX_SPI2_Init(void);