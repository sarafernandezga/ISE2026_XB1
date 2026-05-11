#include "sensordistancia.h"
#include "Driver_I2C.h"
#include <stdio.h>
#include "VL53L0X.h"
#include "BAJOCONSUMO.h"

// --- VARIABLES DEL RTOS ---
osThreadId_t tid_Control_sensor;
osMessageQueueId_t VL_Queue;
static osEventFlagsId_t busyEventFlag_vl;

static const osThreadAttr_t attr_sensor = {
  .name = "VL53L0X_Thread",
  .stack_size = 1024, // Damos más stack porque esta librería pasa muchos structs por valor
  .priority = (osPriority_t) osPriorityNormal,
};

// --- DRIVER I2C (CMSIS) ---
extern ARM_DRIVER_I2C Driver_I2C1;
static ARM_DRIVER_I2C *I2Cdrv = &Driver_I2C1;

// --- INSTANCIA DEL SENSOR ---
// Esta es la estructura que la librería requiere para guardar su estado
struct VL53L0X mi_sensor;

// --- PROTOTIPOS INTERNOS ---
void ThControlsensor (void *argument);
static void I2C1_Init(void);
static void I2C_Callback(uint32_t event);


// ============================================================================
// INICIALIZACIÓN DEL SISTEMA
// ============================================================================
int Init_Thsensor(void)
{
	
  busyEventFlag_vl = osEventFlagsNew(NULL);
  if (busyEventFlag_vl == NULL) return -1;

  VL_Queue = osMessageQueueNew(MSGQUEUE_SENS_OBJECTS, sizeof(MSGQUEUE_SENS_t), NULL);
  if (VL_Queue == NULL) return -1;

  tid_Control_sensor = osThreadNew(ThControlsensor, NULL, &attr_sensor);
  if (tid_Control_sensor == NULL) return -1;

  return 0; // Éxito
}


// ============================================================================
// HILO PRINCIPAL DEL SENSOR (RTOS)
// ============================================================================
void ThControlsensor(void *argument)
{
  MSGQUEUE_SENS_t datos;
  uint32_t flags;
  uint8_t sensor_ok = 0U;

  (void)argument;

  osDelay(200U);

  I2C1_Init();

  mi_sensor.address = 0x29;
  mi_sensor.io_2v8 = true;
  mi_sensor.io_timeout = 500;

  if (VL53L0X_init(&mi_sensor)) {
    sensor_ok = 1U;
  } else {
    sensor_ok = 0U;
  }

  while (1) {

    flags = osThreadFlagsWait(VL_EVT_SAMPLE,
                              osFlagsWaitAny,
                              osWaitForever);

    if ((flags & osFlagsError) != 0U) {
      continue;
    }

    LowPower_BusyEnter();

    if (sensor_ok == 1U) {

      uint16_t mm = VL53L0X_readRangeSingleMillimeters(&mi_sensor);

      if (!VL53L0X_timeoutOccurred(&mi_sensor) && mm < 8000U) {
        datos.Distancia = mm;
      } else {
        datos.Distancia = 0xFFFFU;
      }

    } else {
      datos.Distancia = 0xFFFFU;
    }

    osMessageQueueReset(VL_Queue);
    osMessageQueuePut(VL_Queue, &datos, 0U, 0U);

    LowPower_BusyExit();
  }
}


// ============================================================================
// CONFIGURACIÓN Y CALLBACK DEL I2C
// ============================================================================
static void I2C1_Init(void)
{
  I2Cdrv->Initialize(I2C_Callback);
  I2Cdrv->PowerControl(ARM_POWER_FULL);
  // Configuramos el bus a 400 kHz (Fast Mode) para transferencias rápidas
  I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST); 
  I2Cdrv->Control(ARM_I2C_BUS_CLEAR, 0);
}

static void I2C_Callback(uint32_t event)
{
  if (event & ARM_I2C_EVENT_TRANSFER_DONE) {
    osEventFlagsSet(busyEventFlag_vl, 0x01);
  }
  if (event & ARM_I2C_EVENT_BUS_ERROR) {
    osEventFlagsSet(busyEventFlag_vl, 0x02); 
  }
}


// ============================================================================
// FUNCIONES QUE LA LIBRERÍA REQUIERE (Reemplazan a su i2c.c)
// ¡IMPORTANTE! La librería asume que estas existen y devuelven un código de error
// ============================================================================

int i2c_write(uint8_t address, uint8_t *data, uint8_t length)
{
    // Ceder CPU mientras se transmite
    I2Cdrv->MasterTransmit(address, data, length, false);
    
    // Esperamos a que termine o dé error
    uint32_t flags = osEventFlagsWait(busyEventFlag_vl, 0x01 | 0x02, osFlagsWaitAny, osWaitForever);
    
    if (flags & 0x02) return -1; // Hubo error de bus
    return 0; // Éxito
}

int i2c_read(uint8_t address, uint8_t *data, uint8_t length)
{
    // Ceder CPU mientras recibe
    I2Cdrv->MasterReceive(address, data, length, false);
    
    // Esperamos a que termine o dé error
    uint32_t flags = osEventFlagsWait(busyEventFlag_vl, 0x01 | 0x02, osFlagsWaitAny, osWaitForever);
    
    if (flags & 0x02) return -1; // Hubo error de bus
    return 0; // Éxito
}

void SensorDistancia_RequestSample(void)
{
  if (tid_Control_sensor != NULL) {
    osThreadFlagsSet(tid_Control_sensor, VL_EVT_SAMPLE);
  }
}
