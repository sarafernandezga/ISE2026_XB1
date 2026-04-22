#include "stm32f4xx_hal.h"
#ifndef __ADC_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define RESOLUTION_12B 4096U
#define VREF 3.3f

#define MSGQUEUE_ADC_OBJECTS  1

typedef struct {
  float canal10;
  float canal13;
} MSGQUEUE_ADC_t;

extern osMessageQueueId_t adc_Queue;         

int Init_ThADC (void);

	void ADC1_pins_F429ZI_config(void);
	int ADC_Init_Single_Conversion(ADC_HandleTypeDef *, ADC_TypeDef  *);
	float ADC_getVoltage(ADC_HandleTypeDef * , uint32_t );
#endif

