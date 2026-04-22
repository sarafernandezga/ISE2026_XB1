#ifndef __adc_H
#define __adc_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define POT_MIN_PESO     0.0f
#define POT_MAX_PESO    1000.0f
#define POT_ADC_MAX   4095.0f

#define MSGQUEUE_POT_OBJECTS  1

typedef struct {
  uint16_t peso;   /* peso  */
	uint16_t consumo; // potencia
} MSGQUEUE_POT_t;

extern osMessageQueueId_t pot_Queue;         

int Init_ThPot (void); 

#endif /* __POTENCIOMETRO_H */