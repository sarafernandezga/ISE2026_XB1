#include "PrincipalServidor.h"

#include "COM.h"
#include "lcd.h"
#include "rtc.h"
#include "mem.h"

#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------
 * Thread principal del servidor
 *---------------------------------------------------------*/

osThreadId_t tid_ThPrincipalServidor;

static const osThreadAttr_t attr_principal_servidor = {
  .name = "PrincipalServidor",
  .stack_size = 1024,
  .priority = osPriorityNormal
};

static void ThPrincipalServidor(void *argument);

/*----------------------------------------------------------
 * Estados
 *---------------------------------------------------------*/

#define ESTADO_OK                 0x01U
#define ESTADO_ERROR_HUM          0x02U
#define ESTADO_ERROR_DIST         0x04U

#define DISTANCIA_ERROR_CM        255U

/* Umbrales del proyecto */
#define HUMEDAD_ALTA              70U
#define DEPOSITO_BAJO_CM          4U
#define PESO_BAJO_DECIGRAMOS      5U     /* 5 equivale a 50 g si el cliente manda peso/10 */

//MEMORIA
#define EEPROM_ADDR_CONFIG      0x0000
#define EEPROM_CONFIG_MAGIC     0xA5

#define EEPROM_ADDR_LOG_HDR       0x0100
#define EEPROM_ADDR_LOG_ENTRIES   0x0110
/*
 * Cada entrada real ocupa sizeof(SERVIDOR_LOG_t), pero reservamos 16 bytes
 * por entrada para evitar problemas al cruzar paginas de la EEPROM.
 */
#define EEPROM_LOG_SLOT_SIZE      16U

#define EEPROM_LOG_MAGIC          0x4C  /* 'L' */


#define ESTADO_SLEEP               0x08U

typedef struct {
  uint8_t magic;
  uint8_t auto_enable;
  uint8_t hora1_h;
  uint8_t hora1_m;
  uint8_t hora2_h;
  uint8_t hora2_m;
} EEPROM_CONFIG_t;

typedef struct {
  uint8_t magic;
  uint8_t count;
  uint8_t head;
  uint8_t reserved;
} EEPROM_LOG_HDR_t;

/*----------------------------------------------------------
 * Variables globales internas
 *---------------------------------------------------------*/

static SERVIDOR_DATOS_t datos_actuales = {0};
static SERVIDOR_CONFIG_t config_actual = {
  .auto_enable = 0,

  .hora1_h = 9,
  .hora1_m = 0,

  .hora2_h = 20,
  .hora2_m = 0
};

static volatile uint8_t peticion_dispensar = 0;

static volatile uint8_t peticion_dispensar_modo = SERVIDOR_LOG_MODO_MANUAL;

static SERVIDOR_LOG_t log_ram[SERVIDOR_LOG_MAX];
static uint8_t log_count = 0U;
static uint8_t log_head  = 0U;  /* Proxima posicion a escribir */

static char estado_txt[32];
static char alerta_txt[96];

/* Para evitar dispensar muchas veces dentro del mismo minuto */
static int8_t ultima_hora_auto = -1;
static int8_t ultimo_min_auto  = -1;

static void EEPROM_CargarConfig(void);
static void EEPROM_GuardarConfig(void);
static void EEPROM_CargarLog(void);
static void EEPROM_GuardarLogHeader(void);
static void EEPROM_GuardarLogEntry(uint8_t idx);

static void PrincipalServidor_LogAgregar(uint8_t modo);

/*----------------------------------------------------------
 * Funciones auxiliares
 *---------------------------------------------------------*/

static void EnviarOrdenCliente(uint8_t dispensar)
{
  MSGQUEUE_Data_to_client_t orden;

  if (cola_entrada == NULL) {
    return;
  }

  orden.dispensar = dispensar;
  orden.ack = 0U;

  osMessageQueuePut(cola_entrada, &orden, 0U, 0U);
}



static void ComprobarProgramacionAutomatica(void)
{
  RTC_DateTypeDef fecha;
  RTC_TimeTypeDef hora;

  if (config_actual.auto_enable == 0U) {
    return;
  }

  if (RTC_GetDateTime(&fecha, &hora) != HAL_OK) {
    return;
  }

  uint8_t h = hora.Hours;
  uint8_t m = hora.Minutes;

  /*
   * Evita que, durante el mismo minuto, mande 60 órdenes.
   */
  if ((ultima_hora_auto == (int8_t)h) && (ultimo_min_auto == (int8_t)m)) {
    return;
  }

  if (((h == config_actual.hora1_h) && (m == config_actual.hora1_m)) ||
      ((h == config_actual.hora2_h) && (m == config_actual.hora2_m))) {

		peticion_dispensar_modo = SERVIDOR_LOG_MODO_AUTOMATICO;
		peticion_dispensar = 1U;

		ultima_hora_auto = (int8_t)h;
		ultimo_min_auto  = (int8_t)m;
  }
}

/*----------------------------------------------------------
 * Inicialización
 *---------------------------------------------------------*/

int Init_ThPrincipalServidor(void)
{
  if (Init_ThCom() != 0) {
    return -1;
  }
	if (Init_ThEEPROM() != 0) {
		return -1;
	}

  tid_ThPrincipalServidor = osThreadNew(ThPrincipalServidor, NULL, &attr_principal_servidor);

  if (tid_ThPrincipalServidor == NULL) {
    return -1;
  }

  return 0;
}

/*----------------------------------------------------------
 * Thread principal
 *---------------------------------------------------------*/

static void ThPrincipalServidor(void *argument)
{
  MSGQUEUE_Data_to_server_t rx;
  static uint16_t aux;
  (void)argument;

  /*
   * Damos tiempo a que COM cree sus colas internas.
   */
  osDelay(300U);

	EEPROM_CargarConfig();
	EEPROM_CargarLog();
	
  while (1)
  {
    /*--------------------------------------------
     * 1. Recibir datos desde el cliente
     *-------------------------------------------*/

    if (cola_salida != NULL) {
      while (osMessageQueueGet(cola_salida, &rx, NULL, 0U) == osOK) {

        datos_actuales.humedad     = rx.humedad;
        datos_actuales.distancia   = rx.Distancia;
        datos_actuales.estado      = rx.Estado;
        aux = rx.consumo;
        datos_actuales.consumo     = aux*10;
        datos_actuales.peso        = rx.peso;
        datos_actuales.ack_cliente = rx.ack;
        datos_actuales.timestamp_ms = osKernelGetTickCount();
      }
    }

    /*--------------------------------------------
     * 2. Comprobar dispensación automática
     *-------------------------------------------*/

    ComprobarProgramacionAutomatica();

    /*--------------------------------------------
     * 3. Enviar orden de dispensación si procede
     *-------------------------------------------*/

    if (peticion_dispensar != 0U) {
			uint8_t modo;

			modo = peticion_dispensar_modo;
			peticion_dispensar = 0U;

			EnviarOrdenCliente(1U);

			/*
			* Guardamos el evento cuando el servidor manda la orden.
			*/
			PrincipalServidor_LogAgregar(modo);
		}

    /*--------------------------------------------
     * 4. Actualizar LCD con hora/fecha
     *-------------------------------------------*/

//    if ((osKernelGetTickCount() - last_lcd_update) >= 1000U) {
//      last_lcd_update = osKernelGetTickCount();
//      ActualizarLCD();
//    }

    osDelay(100U);
  }
}

/*----------------------------------------------------------
 * Funciones públicas para CGI
 *---------------------------------------------------------*/

void PrincipalServidor_DispensarManual(void)
{
  peticion_dispensar_modo = SERVIDOR_LOG_MODO_MANUAL;
	peticion_dispensar = 1U;
}

void PrincipalServidor_SetAuto(uint8_t enable)
{
  config_actual.auto_enable = enable ? 1U : 0U;
	EEPROM_GuardarConfig();
}

uint8_t PrincipalServidor_GetAuto(void)
{
  return config_actual.auto_enable;
}

void PrincipalServidor_SetHora1(uint8_t h, uint8_t m)
{
  if (h < 24U && m < 60U) {
    config_actual.hora1_h = h;
    config_actual.hora1_m = m;
		EEPROM_GuardarConfig();
  }
}

void PrincipalServidor_SetHora2(uint8_t h, uint8_t m)
{
  if (h < 24U && m < 60U) {
    config_actual.hora2_h = h;
    config_actual.hora2_m = m;
		EEPROM_GuardarConfig();
  }
}

void PrincipalServidor_GetHora1(uint8_t *h, uint8_t *m)
{
  if (h != NULL) *h = config_actual.hora1_h;
  if (m != NULL) *m = config_actual.hora1_m;
}

void PrincipalServidor_GetHora2(uint8_t *h, uint8_t *m)
{
  if (h != NULL) *h = config_actual.hora2_h;
  if (m != NULL) *m = config_actual.hora2_m;
}

SERVIDOR_DATOS_t PrincipalServidor_GetDatos(void)
{
  return datos_actuales;
}

const char *PrincipalServidor_GetEstadoTexto(void)
{
  if (datos_actuales.estado & ESTADO_ERROR_HUM) {
    snprintf(estado_txt, sizeof(estado_txt), "Error humedad");
  }
  else if (datos_actuales.estado & ESTADO_SLEEP) {
    snprintf(estado_txt, sizeof(estado_txt), "Bajo consumo");
  }
  else {
    snprintf(estado_txt, sizeof(estado_txt), "OK");
  }

  return estado_txt;
}

const char *PrincipalServidor_GetAlertaTexto(void)
{
  if (datos_actuales.humedad >= HUMEDAD_ALTA) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Humedad alta. Revisar ventilacion del deposito.");
  }
  else if (datos_actuales.distancia >= DEPOSITO_BAJO_CM && datos_actuales.distancia != DISTANCIA_ERROR_CM) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Nivel de comida bajo. Rellenar deposito.");
  }
  else if (datos_actuales.peso <= PESO_BAJO_DECIGRAMOS) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Poca comida en el cuenco.");
  }
  else {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Sistema funcionando correctamente.");
  }

  return alerta_txt;
}

static void EEPROM_GuardarConfig(void)
{
  static EEPROM_CONFIG_t eeprom_cfg;
  MSGQUEUE_EEPROM_t msg;

  if (EEPROM_Queue_R == NULL) {
    return;
  }

  eeprom_cfg.magic       = EEPROM_CONFIG_MAGIC;
  eeprom_cfg.auto_enable = config_actual.auto_enable;
  eeprom_cfg.hora1_h     = config_actual.hora1_h;
  eeprom_cfg.hora1_m     = config_actual.hora1_m;
  eeprom_cfg.hora2_h     = config_actual.hora2_h;
  eeprom_cfg.hora2_m     = config_actual.hora2_m;

  msg.op_type  = EEPROM_OP_WRITE;
  msg.mem_addr = EEPROM_ADDR_CONFIG;
  msg.data_ptr = (uint8_t *)&eeprom_cfg;
  msg.length   = sizeof(eeprom_cfg);

  osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);
}


static void EEPROM_CargarConfig(void)
{
  static EEPROM_CONFIG_t eeprom_cfg;
  MSGQUEUE_EEPROM_t msg;

  if ((EEPROM_Queue_R == NULL) || (EEPROM_Queue_S == NULL)) {
    return;
  }

  memset(&eeprom_cfg, 0, sizeof(eeprom_cfg));

  msg.op_type  = EEPROM_OP_READ;
  msg.mem_addr = EEPROM_ADDR_CONFIG;
  msg.data_ptr = (uint8_t *)&eeprom_cfg;
  msg.length   = sizeof(eeprom_cfg);

  osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);

  if (osMessageQueueGet(EEPROM_Queue_S, &msg, NULL, 200U) == osOK) {

    if (eeprom_cfg.magic == EEPROM_CONFIG_MAGIC) {

      if (eeprom_cfg.hora1_h < 24U && eeprom_cfg.hora1_m < 60U &&
          eeprom_cfg.hora2_h < 24U && eeprom_cfg.hora2_m < 60U) {

        config_actual.auto_enable = eeprom_cfg.auto_enable ? 1U : 0U;
        config_actual.hora1_h     = eeprom_cfg.hora1_h;
        config_actual.hora1_m     = eeprom_cfg.hora1_m;
        config_actual.hora2_h     = eeprom_cfg.hora2_h;
        config_actual.hora2_m     = eeprom_cfg.hora2_m;
      }
    }
  }
}

/*----------------------------------------------------------
 * Log de eventos en EEPROM
 *---------------------------------------------------------*/

static uint16_t EEPROM_LogEntryAddr(uint8_t idx)
{
  return (uint16_t)(EEPROM_ADDR_LOG_ENTRIES + ((uint16_t)idx * EEPROM_LOG_SLOT_SIZE));
}

static void EEPROM_GuardarLogHeader(void)
{
  static EEPROM_LOG_HDR_t hdr;
  MSGQUEUE_EEPROM_t msg;

  if (EEPROM_Queue_R == NULL) {
    return;
  }

  hdr.magic    = EEPROM_LOG_MAGIC;
  hdr.count    = log_count;
  hdr.head     = log_head;
  hdr.reserved = 0U;

  msg.op_type  = EEPROM_OP_WRITE;
  msg.mem_addr = EEPROM_ADDR_LOG_HDR;
  msg.data_ptr = (uint8_t *)&hdr;
  msg.length   = sizeof(hdr);

  osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);
}

static void EEPROM_GuardarLogEntry(uint8_t idx)
{
  MSGQUEUE_EEPROM_t msg;

  if ((EEPROM_Queue_R == NULL) || (idx >= SERVIDOR_LOG_MAX)) {
    return;
  }

  msg.op_type  = EEPROM_OP_WRITE;
  msg.mem_addr = EEPROM_LogEntryAddr(idx);
  msg.data_ptr = (uint8_t *)&log_ram[idx];
  msg.length   = sizeof(SERVIDOR_LOG_t);

  osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);
}

static void EEPROM_CargarLog(void)
{
  EEPROM_LOG_HDR_t hdr;
  MSGQUEUE_EEPROM_t msg;
  uint8_t i;

  if ((EEPROM_Queue_R == NULL) || (EEPROM_Queue_S == NULL)) {
    return;
  }

  memset(log_ram, 0, sizeof(log_ram));
  log_count = 0U;
  log_head  = 0U;

  memset(&hdr, 0, sizeof(hdr));

  msg.op_type  = EEPROM_OP_READ;
  msg.mem_addr = EEPROM_ADDR_LOG_HDR;
  msg.data_ptr = (uint8_t *)&hdr;
  msg.length   = sizeof(hdr);

  osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);

  if (osMessageQueueGet(EEPROM_Queue_S, &msg, NULL, 200U) != osOK) {
    return;
  }

  if ((hdr.magic != EEPROM_LOG_MAGIC) ||
      (hdr.count > SERVIDOR_LOG_MAX) ||
      (hdr.head >= SERVIDOR_LOG_MAX)) {

    log_count = 0U;
    log_head  = 0U;
    EEPROM_GuardarLogHeader();
    return;
  }

  log_count = hdr.count;
  log_head  = hdr.head;

  for (i = 0U; i < SERVIDOR_LOG_MAX; i++) {
    msg.op_type  = EEPROM_OP_READ;
    msg.mem_addr = EEPROM_LogEntryAddr(i);
    msg.data_ptr = (uint8_t *)&log_ram[i];
    msg.length   = sizeof(SERVIDOR_LOG_t);

    osMessageQueuePut(EEPROM_Queue_R, &msg, 0U, 0U);

    if (osMessageQueueGet(EEPROM_Queue_S, &msg, NULL, 200U) != osOK) {
      break;
    }
  }
}

static void PrincipalServidor_LogAgregar(uint8_t modo)
{
  RTC_DateTypeDef fecha;
  RTC_TimeTypeDef hora;
  SERVIDOR_LOG_t entrada;
  uint8_t pos;

  memset(&entrada, 0, sizeof(entrada));

  entrada.valid  = 1U;
  entrada.evento = SERVIDOR_LOG_EVENTO_DISPENSAR;
  entrada.modo   = modo;

  if (RTC_GetDateTime(&fecha, &hora) == HAL_OK) {
    entrada.year   = fecha.Year;
    entrada.month  = fecha.Month;
    entrada.day    = fecha.Date;
    entrada.hour   = hora.Hours;
    entrada.minute = hora.Minutes;
    entrada.second = hora.Seconds;
  }

  pos = log_head;

  log_ram[pos] = entrada;

  log_head++;
  if (log_head >= SERVIDOR_LOG_MAX) {
    log_head = 0U;
  }

  if (log_count < SERVIDOR_LOG_MAX) {
    log_count++;
  }

  /*
   * Primero escribimos la entrada y despues la cabecera.
   * Asi la cabecera apunta a un estado ya actualizado.
   */
  EEPROM_GuardarLogEntry(pos);
  EEPROM_GuardarLogHeader();
}

uint8_t PrincipalServidor_LogGetCount(void)
{
  return log_count;
}

/*
 * index = 0 devuelve el evento mas reciente.
 * index = 1 el anterior, etc.
 */
int PrincipalServidor_LogGetByIndex(uint8_t index, SERVIDOR_LOG_t *out)
{
  int16_t pos;

  if ((out == NULL) || (index >= log_count)) {
    return -1;
  }

  pos = (int16_t)log_head - 1 - (int16_t)index;

  while (pos < 0) {
    pos += SERVIDOR_LOG_MAX;
  }

  *out = log_ram[(uint8_t)pos];

  return 0;
}

void PrincipalServidor_LogClear(void)
{
  memset(log_ram, 0, sizeof(log_ram));

  log_count = 0U;
  log_head  = 0U;

  EEPROM_GuardarLogHeader();
}

const char *PrincipalServidor_LogEventoTexto(uint8_t evento)
{
  switch (evento) {
    case SERVIDOR_LOG_EVENTO_DISPENSAR:
      return "Se ha dispensado comida";

    default:
      return "Evento desconocido";
  }
}

const char *PrincipalServidor_LogModoTexto(uint8_t modo)
{
  switch (modo) {
    case SERVIDOR_LOG_MODO_MANUAL:
      return "Manual";

    case SERVIDOR_LOG_MODO_AUTOMATICO:
      return "Automatico";

    default:
      return "Desconocido";
  }
}
