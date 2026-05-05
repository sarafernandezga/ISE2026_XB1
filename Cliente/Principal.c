#include "Principal.h"

#include "COM.h"
#include "PWM.h"
#include "adc.h"
#include "sensordistancia.h"
#include "spih.h"

/*----------------------------------------------------------
 * Thread principal de integración cliente
 *---------------------------------------------------------*/

osThreadId_t tid_ThPrincipal;


static void ThPrincipal(void *argument);

/*----------------------------------------------------------
 * Parámetros de funcionamiento
 *---------------------------------------------------------*/

#define PRINCIPAL_PERIOD_MS        1000U
#define PWM_DISPENSAR_DUTY        70U

#define DISTANCIA_ERROR_VALUE     0xFFFFU
#define ESTADO_OK                 0x01U
#define ESTADO_ERROR_HUM          0x02U
#define ESTADO_ERROR_DIST         0x04U

/*----------------------------------------------------------
 * Funciones auxiliares
 *---------------------------------------------------------*/

static uint8_t Saturar_U8(uint32_t value)
{
  if (value > 255U) return 255U;
  return (uint8_t)value;
}

static uint8_t Humedad_To_U8(float humedad)
{
  if (humedad < 0.0f) humedad = 0.0f;
  if (humedad > 100.0f) humedad = 100.0f;

  return (uint8_t)(humedad + 0.5f);
}

/*
 * El VL53L0X entrega distancia en mm.
 * Como la trama solo tiene uint8_t, se manda en cm.
 * Ejemplo: 1230 mm -> 123 cm.
 */
static uint8_t Distancia_To_U8(uint16_t distancia_mm)
{
  if (distancia_mm == DISTANCIA_ERROR_VALUE) {
    return 255U;
  }

  return Saturar_U8(distancia_mm / 10U);
}

/*
 * El ADC entrega peso 0-1000 g.
 * Como la trama solo tiene uint8_t, se manda en decenas de gramos.
 * Ejemplo: 540 g -> 54.
 */
static uint8_t Peso_To_U8(uint16_t peso_g)
{
  return Saturar_U8(peso_g / 10U);
}

/*
 * Si más adelante medís consumo real, cambiar esta función.
 * Ahora mismo consumo no se está calculando en ADC.c.
 */
static uint8_t Consumo_To_U8(uint16_t consumo)
{
  return Saturar_U8(consumo);
}

/*----------------------------------------------------------
 * Inicialización principal
 *---------------------------------------------------------*/

int Init_ThPrincipal(void)
{
  if (Init_ThCom() != 0) {
    return -1;
  }

  if (Init_ThPWM() != 0) {
    return -1;
  }

  if (Init_ThPot() != 0) {
    return -1;
  }

  if (Init_Thsensor() != 0) {
    return -1;
  }

  if (Init_ThHum() != 0) {
    return -1;
  }

  tid_ThPrincipal = osThreadNew(ThPrincipal, NULL, NULL);

  if (tid_ThPrincipal == NULL) {
    return -1;
  }

  return 0;
}

/*----------------------------------------------------------
 * Hilo principal
 *---------------------------------------------------------*/

static void ThPrincipal(void *argument)
{
  MSGQUEUE_Data_to_server_t trama_tx;
  MSGQUEUE_Data_to_client_t trama_rx;

  MSGQUEUE_POT_t pot_msg;
  MSGQUEUE_SENS_t dist_msg;
  MSGQUEUE_HUM_t hum_msg;
  MSGQUEUE_PWM_t pwm_msg;

  uint16_t peso_actual = 0;
  uint16_t consumo_actual = 0;
  uint16_t distancia_actual = DISTANCIA_ERROR_VALUE;
  float humedad_actual = 0.0f;

  uint8_t estado = ESTADO_OK;

  /*
   * Espera corta para que los hilos de los módulos creen sus colas.
   * Sería más limpio crear todas las colas dentro de Init_ThXXX(),
   * antes de crear los threads.
   */
  osDelay(200U);

  while (1)
  {
    estado = ESTADO_OK;

    /*--------------------------------------------
     * 1. Recibir órdenes desde el servidor
     *-------------------------------------------*/

    if (cola_salida != NULL) {
      while (osMessageQueueGet(cola_salida, &trama_rx, NULL, 0U) == osOK) {

        if (trama_rx.dispensar != 0U) {
          pwm_msg.duty = PWM_DISPENSAR_DUTY;

          if (pwm_Queue != NULL) {
            osMessageQueuePut(pwm_Queue, &pwm_msg, 0U, 0U);
          }
        }
      }
    }

    /*--------------------------------------------
     * 2. Leer última muestra de peso/consumo
     *-------------------------------------------*/

    if (pot_Queue != NULL) {
      while (osMessageQueueGet(pot_Queue, &pot_msg, NULL, 0U) == osOK) {
        peso_actual = pot_msg.peso;
        consumo_actual = pot_msg.consumo;
      }
    }

    /*--------------------------------------------
     * 3. Leer última distancia
     *-------------------------------------------*/

    if (VL_Queue != NULL) {
      while (osMessageQueueGet(VL_Queue, &dist_msg, NULL, 0U) == osOK) {
        distancia_actual = dist_msg.Distancia;
      }
    }

    if (distancia_actual == DISTANCIA_ERROR_VALUE) {
      estado |= ESTADO_ERROR_DIST;
    }

    /*--------------------------------------------
     * 4. Leer última humedad
     *-------------------------------------------*/

    if (hum_Queue != NULL) {
      while (osMessageQueueGet(hum_Queue, &hum_msg, NULL, 0U) == osOK) {
        humedad_actual = hum_msg.cmd;
      }
    }

    if (BME280_IsInitialized() == 0U) {
      estado |= ESTADO_ERROR_HUM;
    }

    /*--------------------------------------------
     * 5. Preparar trama para el servidor
     *-------------------------------------------*/

    trama_tx.consumo   = Consumo_To_U8(consumo_actual);
    trama_tx.Distancia = Distancia_To_U8(distancia_actual);
    trama_tx.humedad   = Humedad_To_U8(humedad_actual);
    trama_tx.peso      = Peso_To_U8(peso_actual);
    trama_tx.Estado    = estado;
    trama_tx.ack       = 0U;

    /*--------------------------------------------
     * 6. Enviar trama al módulo COM
     *-------------------------------------------*/

    if (cola_entrada != NULL) {
      osMessageQueuePut(cola_entrada, &trama_tx, 0U, 0U);
    }

    osDelay(PRINCIPAL_PERIOD_MS);
  }
}