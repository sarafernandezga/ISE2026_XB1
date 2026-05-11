#ifndef __ADC_H
#define __ADC_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

#define MSGQUEUE_ADC_OBJECTS  1

#define ADC_RESOLUTION_12B   4095.0f
#define ADC_VREF             3.3f

#define SHUNT_RESISTOR       0.1f
#define CURRENT_GAIN         60.0f
#define MAX_WEIGHT_G         1000.0f

#define ADC_EVT_SAMPLE       0x01U

typedef struct {
  uint16_t consumo;
  uint16_t peso;
} MSGQUEUE_POT_t;

extern osMessageQueueId_t pot_Queue;

int Init_ThPot(void);
void ADC_RequestSample(void);

#endif