#ifndef __COM_H
#define __COM_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Driver_USART.h"

int Init_ThCom(void);                                                  // Funcion de creacion e inicializacion del thread asociado al CLK

#define SOH 0x7E
#define EOT 0xEF
#define Flag_Recibido 0x01
#define Flag_Recibido2 0x02
#define BUFFER_SIZE 64

typedef enum{
	InitState,
	DefaultState
	
}Estado_t;

typedef struct {
  uint8_t humedad;
	uint8_t Distancia;
	uint8_t Estado;
	uint8_t consumo;
	uint8_t peso;
	uint8_t ack;
} MSGQUEUE_Data_to_server_t; 

typedef struct {
  uint8_t dispensar;
	uint8_t ack;
} MSGQUEUE_Data_to_client_t;

#endif
