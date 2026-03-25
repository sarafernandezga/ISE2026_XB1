#ifndef __thCLK_H
#define __thCLK_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int Init_ThClk (void);                                                  // Funcion de creacion e inicializacion del thread asociado al CLK

typedef struct {
  uint8_t Segundos;
	uint8_t Minutos;
	uint8_t Horas;
} Hora_t;

#endif
