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
#define DEPOSITO_BAJO_CM          20U
#define PESO_BAJO_DECIGRAMOS      5U     /* 5 equivale a 50 g si el cliente manda peso/10 */

//MEMORIA
#define EEPROM_ADDR_CONFIG      0x0000
#define EEPROM_CONFIG_MAGIC     0xA5

typedef struct {
  uint8_t magic;
  uint8_t auto_enable;
  uint8_t hora1_h;
  uint8_t hora1_m;
  uint8_t hora2_h;
  uint8_t hora2_m;
} EEPROM_CONFIG_t;
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

static char estado_txt[32];
static char alerta_txt[96];

/* Para evitar dispensar muchas veces dentro del mismo minuto */
static int8_t ultima_hora_auto = -1;
static int8_t ultimo_min_auto  = -1;

static void EEPROM_CargarConfig(void);
static void EEPROM_GuardarConfig(void);

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
      EnviarOrdenCliente(1U);
      peticion_dispensar = 0U;
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
  else if (datos_actuales.estado & ESTADO_ERROR_DIST) {
    snprintf(estado_txt, sizeof(estado_txt), "Error distancia");
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
  else if (datos_actuales.distancia <= DEPOSITO_BAJO_CM) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Nivel de comida bajo. Rellenar deposito.");
  }
  else if (datos_actuales.peso <= PESO_BAJO_DECIGRAMOS) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Poca comida en el cuenco.");
  }
  else if (datos_actuales.distancia == DISTANCIA_ERROR_CM) {
    snprintf(alerta_txt, sizeof(alerta_txt),
             "Sensor de distancia sin lectura valida.");
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

