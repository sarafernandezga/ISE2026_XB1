#ifndef SENSORDISTANCIA_H
#define SENSORDISTANCIA_H

#include <stdint.h>
#include "cmsis_os2.h"

#define MSGQUEUE_SENS_OBJECTS 1
#define VL_EVT_SAMPLE         0x01U

typedef struct {
  uint16_t Distancia;
} MSGQUEUE_SENS_t;

extern osMessageQueueId_t VL_Queue;

int Init_Thsensor(void);
void SensorDistancia_RequestSample(void);

#endif