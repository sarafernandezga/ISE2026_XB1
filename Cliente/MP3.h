#ifndef __MP3_H
#define __MP3_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Driver_USART.h"

int Init_ThMP3(void);                                                  // Funcion de creacion e inicializacion del thread asociado al CLK


#define Flag_Recibido 0x01
#define Flag_Recibido2 0x02
#define BUFFER_SIZEMP3 14

typedef enum{
	InitStateMP3,
	DefaultStateMP3
	
}EstadoMP3_t;

typedef struct{
	uint8_t accion4;
	uint8_t directory6;
	uint8_t cantidad7;
}MSG_MP3_t;


#endif