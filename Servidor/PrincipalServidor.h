#ifndef __PRINCIPAL_SERVIDOR_H
#define __PRINCIPAL_SERVIDOR_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

#define SERVIDOR_LOG_MAX                 15U

#define SERVIDOR_LOG_EVENTO_DISPENSAR    1U

#define SERVIDOR_LOG_MODO_MANUAL         1U
#define SERVIDOR_LOG_MODO_AUTOMATICO     2U

typedef struct {
  uint8_t humedad;       /* % */
  uint8_t distancia;     /* cm */
  uint8_t estado;
  uint16_t consumo;      /* mA */
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

/* Entrada de log guardada en EEPROM */
typedef struct {
  uint8_t valid;
  uint8_t year;      /* 0-99 */
  uint8_t month;     /* 1-12 */
  uint8_t day;       /* 1-31 */
  uint8_t hour;      /* 0-23 */
  uint8_t minute;    /* 0-59 */
  uint8_t second;    /* 0-59 */
  uint8_t evento;    /* SERVIDOR_LOG_EVENTO_x */
  uint8_t modo;      /* manual / automatico */
} SERVIDOR_LOG_t;

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

/* Funciones para la web de log */
uint8_t PrincipalServidor_LogGetCount(void);
int PrincipalServidor_LogGetByIndex(uint8_t index, SERVIDOR_LOG_t *out);
void PrincipalServidor_LogClear(void);

const char *PrincipalServidor_LogEventoTexto(uint8_t evento);
const char *PrincipalServidor_LogModoTexto(uint8_t modo);

#endif