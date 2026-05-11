#ifndef __ADC_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define RESOLUTION_12B 4096U
#define VREF 3.3f

#define MSGQUEUE_ADC_OBJECTS  1

#define ADC_RESOLUTION_12B   4095.0f
#define ADC_VREF             3.3f
#define LOAD_VOLTAGE         9.0f

#define SHUNT_RESISTOR       0.1f
#define CURRENT_GAIN         60.0f

#define WEIGHT_GAIN          660.0f
#define MAX_WEIGHT_G         1000.0f


typedef struct {
  uint8_t consumo;
  float peso;
} MSGQUEUE_POT_t;

extern osMessageQueueId_t pot_Queue;         

int Init_ThPot (void);

	void ADC1_pins_F429ZI_config(void);
	int ADC_Init_Single_Conversion(ADC_HandleTypeDef *, ADC_TypeDef  *);
	float ADC_getVoltage(ADC_HandleTypeDef * , uint32_t );
#endif

