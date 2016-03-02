#include "board.h"

extern __IO uint32_t ADC_DualConvertedValueTab[2];

void adcInit(void (*)(int16_t *));
