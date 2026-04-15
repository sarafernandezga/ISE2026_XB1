#ifndef SENSORDISTANCIA_H
#define SENSORDISTANCIA_H

#include <stdint.h>
#include "cmsis_os2.h"

// --- DEFINICIONES ---
#define MSGQUEUE_SENS_OBJECTS 16 // Tamaño de la cola de mensajes

// --- ESTRUCTURAS DE DATOS ---
typedef struct {
    uint16_t Distancia; // Distancia medida en milímetros
} MSGQUEUE_SENS_t;

// --- VARIABLES EXTERNAS ---
extern osMessageQueueId_t VL_Queue;

// --- PROTOTIPOS DE FUNCIONES ---
int Init_Thsensor(void);

#endif /* POT_H */