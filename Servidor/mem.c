#include "mem.h"
#include <string.h> 

// Dirección I2C base de la EEPROM (A0 y A1 a GND)
#define AT24C_ADDR 0x50 

extern ARM_DRIVER_I2C Driver_I2C1;
static ARM_DRIVER_I2C *I2Cdrv = &Driver_I2C1;
static void I2C_Callback(uint32_t event);
static osEventFlagsId_t busyEventFlag_eeprom;

static void I2C1_Init(void)
{
  I2Cdrv->Initialize(I2C_Callback);
  I2Cdrv->PowerControl(ARM_POWER_FULL);
  I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST); 
  I2Cdrv->Control(ARM_I2C_BUS_CLEAR, 0);
}

static void I2C_Callback(uint32_t event)
{
  if (event & ARM_I2C_EVENT_TRANSFER_DONE)
    osEventFlagsSet(busyEventFlag_eeprom, 0x01);
  if (event & ARM_I2C_EVENT_BUS_ERROR)
    osEventFlagsSet(busyEventFlag_eeprom, 0x02);
}

/**
 * Lee datos de la EEPROM (Random / Sequential Read)
 * @param mem_addr Dirección de memoria interna (0x0000 a 0x3FFF para 128K o 0x7FFF para 256K)
 * @param buffer Puntero donde se guardarán los datos leídos
 * @param length Cantidad de bytes a leer
 */
static void Read_EEPROM(uint16_t mem_addr, uint8_t *buffer, uint32_t length)
{
  uint8_t addr_buf[2];
  addr_buf[0] = (uint8_t)(mem_addr >> 8);   // Parte Alta de la dirección (MSB)
  addr_buf[1] = (uint8_t)(mem_addr & 0xFF); // Parte Baja de la dirección (LSB)

  I2Cdrv->MasterTransmit(AT24C_ADDR, addr_buf, 2, true);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);

  I2Cdrv->MasterReceive(AT24C_ADDR, buffer, length, false);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);
}

/**
 * Escribe datos en la EEPROM (Byte / Page Write)
 * @param mem_addr Dirección de memoria inicial
 * @param data Puntero a los datos a escribir
 * @param length Cantidad de bytes a escribir (MAX 64 bytes por operación para evitar Roll Over)
 */
static void Write_EEPROM(uint16_t mem_addr, uint8_t *data, uint32_t length)
{
  // El buffer a transmitir debe contener: [MSB] [LSB] [DATA 1] [DATA 2] ...
  // Como máximo una página son 64 bytes + 2 de dirección = 66 bytes.
  uint8_t tx_buf[66]; 
  
  if(length > 64) length = 64; // Protección básica para no desbordar la página

  tx_buf[0] = (uint8_t)(mem_addr >> 8);
  tx_buf[1] = (uint8_t)(mem_addr & 0xFF);
  memcpy(&tx_buf[2], data, length);

  // Enviar todo de una vez (genera STOP al final)
  I2Cdrv->MasterTransmit(AT24C_ADDR, tx_buf, length + 2, false);
  osEventFlagsWait(busyEventFlag_eeprom, 0x01, osFlagsWaitAny, osWaitForever);

  // CRÍTICO: La memoria necesita tiempo físico para guardar los datos en su matriz interna
  osDelay(5); 
}