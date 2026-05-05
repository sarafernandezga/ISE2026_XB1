#include "mem.h"
#include "Driver_I2C.h" // Asegúrate de incluir el driver correcto
#include <string.h>

// --- Variables del RTOS ---
osThreadId_t tid_Control_EEPROM;
osMessageQueueId_t EEPROM_Queue_R;
osMessageQueueId_t EEPROM_Queue_S;

static osEventFlagsId_t busyEventFlag_eeprom;

static const osThreadAttr_t attr_eeprom = {

  .stack_size = 512, // Aumentado ligeramente por el buffer tx_buf de 66 bytes

};

// --- Variables del I2C ---
extern ARM_DRIVER_I2C Driver_I2C1;
static ARM_DRIVER_I2C *I2Cdrv = &Driver_I2C1;

// --- Prototipos privados ---
void ThControlEEPROM(void *argument);
static void I2C1_Init(void);
static void I2C_Callback(uint32_t event);
static void Read_EEPROM(uint16_t mem_addr, uint8_t *buffer, uint32_t length);
static void Write_EEPROM(uint16_t mem_addr, uint8_t *data, uint32_t length);

// --- Funciones de Inicialización ---
int Init_ThEEPROM(void)
{
  // 1. Iniciar hardware
  I2C1_Init();
  busyEventFlag_eeprom = osEventFlagsNew(NULL);
  if (busyEventFlag_eeprom == NULL) return -1;

  // 2. Crear la cola de mensajes
  EEPROM_Queue_S = osMessageQueueNew(MSGQUEUE_EEPROM_OBJECTS, sizeof(MSGQUEUE_EEPROM_t), NULL);
  if (EEPROM_Queue_S == NULL) return -1;
  EEPROM_Queue_R = osMessageQueueNew(MSGQUEUE_EEPROM_OBJECTS, sizeof(MSGQUEUE_EEPROM_t), NULL);
  if (EEPROM_Queue_R == NULL) return -1;
  // 3. Crear el hilo independiente
  tid_Control_EEPROM = osThreadNew(ThControlEEPROM, NULL, &attr_eeprom);
  if (tid_Control_EEPROM == NULL) return -1;

  return 0;
}

// --- Hilo Principal ---
void ThControlEEPROM(void *argument)
{
  MSGQUEUE_EEPROM_t msg;

  while (1) {
    // El hilo se bloquea aquí hasta que recibe un mensaje en la cola
    osStatus_t status = osMessageQueueGet(EEPROM_Queue_R, &msg, NULL, osWaitForever);
    
    if (status == osOK) {
      // Se recibió un mensaje, determinar la acción
      if (msg.op_type == EEPROM_OP_WRITE) {
        Write_EEPROM(msg.mem_addr, msg.data_ptr, msg.length);
      } 
      else if (msg.op_type == EEPROM_OP_READ) {
        Read_EEPROM(msg.mem_addr, msg.data_ptr, msg.length);
				
				osMessageQueuePut(EEPROM_Queue_S, &msg,NULL,NULL);
      }
			
			
    }
		
  }
}

// --- Inicialización del bus I2C ---
static void I2C1_Init(void)
{
  I2Cdrv->Initialize(I2C_Callback);
  I2Cdrv->PowerControl(ARM_POWER_FULL);
  I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST); // 400 kHz
  I2Cdrv->Control(ARM_I2C_BUS_CLEAR, 0);
}

// --- Callback I2C ---
static void I2C_Callback(uint32_t event)
{
  if (event & ARM_I2C_EVENT_TRANSFER_DONE)
    osEventFlagsSet(busyEventFlag_eeprom, 0x01);
  if (event & ARM_I2C_EVENT_BUS_ERROR)
    osEventFlagsSet(busyEventFlag_eeprom, 0x02);
}

// --- Funciones de Bajo Nivel I2C ---
static void Read_EEPROM(uint16_t mem_addr, uint8_t *buffer, uint32_t length)
{
  uint8_t addr_buf[2];
  addr_buf[0] = (uint8_t)(mem_addr >> 8);
  addr_buf[1] = (uint8_t)(mem_addr & 0xFF);

  I2Cdrv->MasterTransmit(AT24C_ADDR, addr_buf, 2, true);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);

  I2Cdrv->MasterReceive(AT24C_ADDR, buffer, length, false);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);
}

static void Write_EEPROM(uint16_t mem_addr, uint8_t *data, uint32_t length)
{
  uint8_t tx_buf[66]; 
  if(length > 64) length = 64; 

  tx_buf[0] = (uint8_t)(mem_addr >> 8);
  tx_buf[1] = (uint8_t)(mem_addr & 0xFF);
  memcpy(&tx_buf[2], data, length);

  I2Cdrv->MasterTransmit(AT24C_ADDR, tx_buf, length + 2, false);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);

  osDelay(5); // Retardo obligatorio de 5ms por el datasheet de la AT24C128
}