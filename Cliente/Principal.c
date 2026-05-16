#include "Principal.h"

#include "COM.h"
#include "PWM.h"
#include "adc.h"
#include "sensordistancia.h"
#include "spih.h"
#include "BAJOCONSUMO.h"

/*----------------------------------------------------------
 * Thread principal de integración cliente
 *---------------------------------------------------------*/

osThreadId_t tid_ThPrincipal;

static void ThPrincipal(void *argument);

/*----------------------------------------------------------
 * Parámetros de funcionamiento
 *---------------------------------------------------------*/

#define DISTANCIA_ERROR_VALUE      0xFFFFU

#define ESTADO_OK                  0x01U
#define ESTADO_ERROR_HUM           0x02U
#define ESTADO_ERROR_DIST          0x04U
#define ESTADO_SLEEP               0x08U

#define SENSOR_TIMEOUT_ADC_MS      100U
#define SENSOR_TIMEOUT_HUM_MS      300U
#define SENSOR_TIMEOUT_VL_MS       700U

#define ACTIVE_WINDOW_MS           10000U
#define ACTIVE_SAMPLE_PERIOD_MS    1000U

#define ESTADO_SLEEP               0x08U

/*
 * En la web queremos mostrar 100 mA.
 * Como el servidor hace rx.consumo * 10,
 * desde el cliente enviamos 10.
 */
#define CONSUMO_SLEEP_TX           10U

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

static uint8_t Distancia_To_U8(uint16_t distancia_mm)
{
  if (distancia_mm == DISTANCIA_ERROR_VALUE) {
    return 255U;
  }

  return Saturar_U8(distancia_mm / 10U);
}

static uint8_t Peso_To_U8(uint16_t peso_g)
{
  return Saturar_U8(peso_g / 10U);
}

static uint8_t Consumo_To_U8(uint16_t consumo_x100)
{
  /*
   * consumo_x100 viene como corriente * 100.
   * Ejemplo: 250 -> 2.50 A.
   * Como la trama es uint8_t, mandamos saturado.
   */
  return Saturar_U8(consumo_x100);
}

static void VaciarColasSensores(void)
{
  MSGQUEUE_POT_t pot_msg;
  MSGQUEUE_SENS_t dist_msg;
  MSGQUEUE_HUM_t hum_msg;

  if (pot_Queue != NULL) {
    while (osMessageQueueGet(pot_Queue, &pot_msg, NULL, 0U) == osOK) {}
  }

  if (VL_Queue != NULL) {
    while (osMessageQueueGet(VL_Queue, &dist_msg, NULL, 0U) == osOK) {}
  }

  if (hum_Queue != NULL) {
    while (osMessageQueueGet(hum_Queue, &hum_msg, NULL, 0U) == osOK) {}
  }
}

/*----------------------------------------------------------
 * Notificación externa de eventos
 *---------------------------------------------------------*/

void Principal_NotifyEvent(uint32_t event_flags)
{
  if (tid_ThPrincipal != NULL) {
    osThreadFlagsSet(tid_ThPrincipal, event_flags);
  }
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

  if (LowPower_Init() != 0) {
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

  uint16_t peso_actual = 0U;
  uint16_t consumo_actual = 0U;
  uint16_t distancia_actual = DISTANCIA_ERROR_VALUE;
  float humedad_actual = 0.0f;

  uint8_t estado;
  uint8_t ack_pendiente;
  uint8_t dispensar_pendiente;

  uint32_t flags;
  uint32_t active_until;
  uint32_t now;
  uint32_t wait_time;
  int32_t remaining_time;

  (void)argument;

  osDelay(500U);

  /*
   * Primera medida nada más arrancar.
   */
  Principal_NotifyEvent(PRINCIPAL_EVT_RTC);

  while (1)
  {
    /*
     * Espera hasta que:
     * - despierte el RTC
     * - llegue una trama desde servidor
     */
    flags = osThreadFlagsWait(PRINCIPAL_EVT_RTC | PRINCIPAL_EVT_COM,
                              osFlagsWaitAny,
                              osWaitForever);

    if ((flags & osFlagsError) != 0U) {
      continue;
    }

    /*
     * Desde este punto mantenemos el sistema despierto.
     */
    LowPower_BusyEnter();

    active_until = osKernelGetTickCount() + ACTIVE_WINDOW_MS;

    /*
     * Ventana activa de 10 segundos.
     * Durante esta ventana se toman medidas periódicamente.
     */
    while ((int32_t)(active_until - osKernelGetTickCount()) > 0)
    {
      estado = ESTADO_OK;
      ack_pendiente = 0U;
      dispensar_pendiente = 0U;

      /*--------------------------------------------
       * 1. Procesar órdenes recibidas del servidor
       *-------------------------------------------*/

      if (cola_salida != NULL) {
        while (osMessageQueueGet(cola_salida, &trama_rx, NULL, 0U) == osOK) {

          if (trama_rx.dispensar != 0U) {
            dispensar_pendiente = 1U;
            ack_pendiente = 1U;

            /*
             * Si llega una orden manual mientras está despierto,
             * ampliamos la ventana activa otros 10 segundos.
             */
            active_until = osKernelGetTickCount() + ACTIVE_WINDOW_MS;
          }
        }
      }

      /*--------------------------------------------
       * 2. Ejecutar dispensación si procede
       *-------------------------------------------*/

      if (dispensar_pendiente != 0U) {

        pwm_msg.cmd = PWM_CMD_DISPENSAR;
        pwm_msg.angle = 0U;
        pwm_msg.hold_ms = 0U;

        if (pwm_Queue != NULL) {
          osMessageQueuePut(pwm_Queue, &pwm_msg, 0U, 0U);
        }
      }

      /*--------------------------------------------
       * 3. Pedir nuevas medidas
       *-------------------------------------------*/

      VaciarColasSensores();

      ADC_RequestSample();
      SensorDistancia_RequestSample();
      BME280_RequestMeasurement();

      /*--------------------------------------------
       * 4. Recoger ADC: peso y consumo
       *-------------------------------------------*/

      if (pot_Queue != NULL) {
        if (osMessageQueueGet(pot_Queue,
                              &pot_msg,
                              NULL,
                              SENSOR_TIMEOUT_ADC_MS) == osOK) {
          peso_actual = pot_msg.peso;
          consumo_actual = pot_msg.consumo;
        }
      }

      /*--------------------------------------------
       * 5. Recoger distancia
       *-------------------------------------------*/

      if (VL_Queue != NULL) {
        if (osMessageQueueGet(VL_Queue,
                              &dist_msg,
                              NULL,
                              SENSOR_TIMEOUT_VL_MS) == osOK) {
          distancia_actual = dist_msg.Distancia;
        } else {
          distancia_actual = DISTANCIA_ERROR_VALUE;
        }
      }


      /*--------------------------------------------
       * 6. Recoger humedad
       *-------------------------------------------*/

      if (hum_Queue != NULL) {
        if (osMessageQueueGet(hum_Queue,
                              &hum_msg,
                              NULL,
                              SENSOR_TIMEOUT_HUM_MS) == osOK) {
          humedad_actual = hum_msg.cmd;
        }
      }

      if (BME280_IsInitialized() == 0U) {
        estado |= ESTADO_ERROR_HUM;
      }

      /*
       * Aquí normalmente LowPower_IsSleeping() será 0,
       * porque estamos en ventana activa.
       */
      if (LowPower_IsSleeping() != 0U) {
        estado |= ESTADO_SLEEP;
      }

      /*--------------------------------------------
       * 7. Preparar trama hacia servidor
       *-------------------------------------------*/

      trama_tx.consumo   = Consumo_To_U8(consumo_actual);
      trama_tx.Distancia = Distancia_To_U8(distancia_actual);
      trama_tx.humedad   = Humedad_To_U8(humedad_actual);
      trama_tx.peso      = Peso_To_U8(peso_actual);
      trama_tx.Estado    = estado;
      trama_tx.ack       = ack_pendiente;

      /*--------------------------------------------
       * 8. Enviar trama al módulo COM
       *-------------------------------------------*/

      if (cola_entrada != NULL) {
        osMessageQueuePut(cola_entrada, &trama_tx, 0U, 0U);
      }

      /*--------------------------------------------
       * 9. Esperar hasta la siguiente medida
       *-------------------------------------------*/

      now = osKernelGetTickCount();
      remaining_time = (int32_t)(active_until - now);

      if (remaining_time <= 0) {
        break;
      }

      if ((uint32_t)remaining_time > ACTIVE_SAMPLE_PERIOD_MS) {
        wait_time = ACTIVE_SAMPLE_PERIOD_MS;
      } else {
        wait_time = (uint32_t)remaining_time;
      }

      /*
       * En vez de hacer osDelay(wait_time), esperamos flags.
       * Así, si llega una orden COM durante la ventana activa,
       * la procesamos antes de que pase el segundo completo.
       */
      flags = osThreadFlagsWait(PRINCIPAL_EVT_RTC | PRINCIPAL_EVT_COM,
                                osFlagsWaitAny,
                                wait_time);

      if (flags == osFlagsErrorTimeout) {
        /*
         * No ha llegado nada nuevo. Se hará otra medida normal.
         */
      }
      else if ((flags & osFlagsError) != 0U) {
        /*
         * Error raro de flags. Salimos de la ventana activa.
         */
        break;
      }
      else {
        /*
         * Si llega COM, ampliamos ventana activa.
         * Si llega RTC mientras ya estamos despiertos, simplemente
         * seguimos midiendo dentro de la ventana actual.
         */
        if ((flags & PRINCIPAL_EVT_COM) != 0U) {
          active_until = osKernelGetTickCount() + ACTIVE_WINDOW_MS;
        }
      }
    }

    /*
     * Fin de los 10 segundos activos.
     * Ahora el hilo de bajo consumo ya puede volver a meter la CPU en Sleep.
     */

		estado |= ESTADO_SLEEP;

		trama_tx.consumo   = CONSUMO_SLEEP_TX;                 /* 10 -> servidor muestra 100 mA */
		trama_tx.Distancia = Distancia_To_U8(distancia_actual);
		trama_tx.humedad   = Humedad_To_U8(humedad_actual);
		trama_tx.peso      = Peso_To_U8(peso_actual);
		trama_tx.Estado    = estado;
		trama_tx.ack       = 0U;

		if (cola_entrada != NULL) {
			osMessageQueuePut(cola_entrada, &trama_tx, 0U, 0U);
		}

		/*
		* Pequeña espera para dar tiempo al hilo COM a sacar la trama por UART
		* antes de liberar el bloqueo de bajo consumo.
		*/
		osDelay(50U);

		/*
		* Ahora sí, el hilo de bajo consumo puede volver a meter la CPU en Sleep.
		*/
		LowPower_BusyExit();
  }
}