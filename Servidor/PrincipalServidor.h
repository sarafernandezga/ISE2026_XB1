#ifndef __PRINCIPAL_SERVIDOR_H
#define __PRINCIPAL_SERVIDOR_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

typedef struct {
  uint8_t humedad;       /* % */
  uint8_t distancia;     /* cm */
  uint8_t estado;
  uint8_t consumo;
  uint8_t peso;          /* decenas de gramos */
  uint8_t ack_cliente;
  uint32_t timestamp_ms;
} SERVIDOR_DATOS_t;

typedef struct {
  uint8_t auto_enable;

  uint8_t hora1_h;
  uint8_t hora1_m;

  uint8_t hora2_h;
  uint8_t hora2_m;
} SERVIDOR_CONFIG_t;

int Init_ThPrincipalServidor(void);

/* Funciones usadas desde la web CGI */
void PrincipalServidor_DispensarManual(void);

void PrincipalServidor_SetAuto(uint8_t enable);
uint8_t PrincipalServidor_GetAuto(void);

void PrincipalServidor_SetHora1(uint8_t h, uint8_t m);
void PrincipalServidor_SetHora2(uint8_t h, uint8_t m);

void PrincipalServidor_GetHora1(uint8_t *h, uint8_t *m);
void PrincipalServidor_GetHora2(uint8_t *h, uint8_t *m);

SERVIDOR_DATOS_t PrincipalServidor_GetDatos(void);

const char *PrincipalServidor_GetEstadoTexto(void);
const char *PrincipalServidor_GetAlertaTexto(void);

#endif
