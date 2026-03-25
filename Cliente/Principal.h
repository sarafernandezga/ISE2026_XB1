#ifndef __Principal_H
#define __Principal_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "com.h"
#include "lcd.h"
#include "thClk.h"
#include "thJoystick.h"
#include "PWM.h"
#include "leds_N.h"
#include "Pot.h"
#include "sensores.h"
#include "MP3.h"

typedef enum{
	Reposo,
	Reproduccion,
	Programacion
	
}Estado_LCD;

typedef enum{
	HORAS,
	MINUTOS,
	SEGUNDOS
	
}Estado_programacion_t;



int Init_ThPrincipal (void);      // Funcion de creacion e inicializacion del thread asociado al Principal

#endif
