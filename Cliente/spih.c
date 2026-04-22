#include "spih.h"
#include <string.h>
#include <stdio.h>
/*----------------------------------------------------------------------------
 *      Thread 'ThHum'
 *---------------------------------------------------------------------------*/

osThreadId_t tid_ThHum;
osMessageQueueId_t hum_Queue;

void ThHum (void *argument);

/* SPI Driver */
extern ARM_DRIVER_SPI Driver_SPI1;
ARM_DRIVER_SPI* SPIdrv = &Driver_SPI1;

/* Flags */
osEventFlagsId_t busyEventFlag;
static void busy(uint32_t event);

/* Atributos del hilo */
const osThreadAttr_t threadHum_attr = {
  .stack_size = 1024
};

/* Mensajes */
MSGQUEUE_HUM_t hum_msg_rec;
MSGQUEUE_HUM_t hum_msg_send;

/* Estado simple del módulo */
static uint8_t bme280_initialized = 0;
static uint16_t humedad_raw = 0;
static float humedad_percent = 0.0f;
static int32_t temperatura_centi = 0;   // 0.01 ºC

/* Buffers SPI */
static uint8_t spi_tx[32];
static uint8_t spi_rx[32];


int Init_ThHum (void)
{
  tid_ThHum = osThreadNew(ThHum, NULL, &threadHum_attr);
  hum_Queue = osMessageQueueNew(4, sizeof(MSGQUEUE_HUM_t), NULL);

  if (tid_ThHum == NULL) {
    return(-1);
  }

  if (hum_Queue == NULL) {
    return(-1);
  }

  return(0);
}

uint8_t BME280_IsInitialized(void)
{
  return bme280_initialized;
}

uint16_t BME280_GetHumidityRaw(void)
{
  return humedad_raw;
}

float BME280_GetHumidityPercent(void)
{
  return humedad_percent;
}

int32_t BME280_GetTemperatureCentiDeg(void)
{
  return temperatura_centi;
}

/* Hilo */
void ThHum (void *argument)
{
  int32_t adc_T, adc_H;

  BME280_Init();

  /* Lanzamos una primera medida */

  while (1)
  {
		
    if (/*hum_msg_rec.cmd == 0 &&*/ bme280_initialized == 1)
    {
      BME280_TriggerMeasurement();

      /* Espera simple hasta fin de conversión */
      while (BME280_ReadStatus() & 0x08) {
        osDelay(1);
      }

      BME280_ReadRawData(&adc_T, &adc_H);

      temperatura_centi = BME280_Compensate_T(adc_T);
      humedad_percent   = BME280_Compensate_H(adc_H);

			printf("%f", humedad_percent);
      if (adc_H < 0) {
        humedad_raw = 0;
      } else if (adc_H > 65535) {
        humedad_raw = 65535;
      } else {
        humedad_raw = (uint16_t)adc_H;
      }

      osDelay(1000);

      hum_msg_send.cmd = humedad_percent;
      osMessageQueuePut(hum_Queue, &hum_msg_send, 0U, 0U);
			
    }
  }
}

/* Inicialización del módulo */
void BME280_Init(void)
{
  uint8_t id;

  BME280_GPIO_Init();

  busyEventFlag = osEventFlagsNew(NULL);

  BME280_SPI_Init();

  BME280_CS_1();

  memset(&bme_cal, 0, sizeof(bme_cal));

  /* tiempo de arranque del sensor */
  osDelay(3);

  id = BME280_ReadID();

  if (id != BME280_CHIP_ID) {
    bme280_initialized = 0;
    return;
  }

  BME280_Reset();
  osDelay(5);

  while (BME280_ReadStatus() & 0x01) {
    osDelay(1);
  }

  BME280_ReadCalibration();
  BME280_Config();

  bme280_initialized = 1;
}

/* GPIO + SPI */
void BME280_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();

  GPIO_InitStruct.Pin = BME280_CS_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(BME280_CS_PORT, &GPIO_InitStruct);
}

void BME280_SPI_Init(void)
{
  SPIdrv->Initialize(busy);
  SPIdrv->PowerControl(ARM_POWER_FULL);

  /* El BME280 admite modo 0 y modo 3. Aquí uso modo 0 */
  SPIdrv->Control(
    ARM_SPI_MODE_MASTER |
    ARM_SPI_CPOL0_CPHA0 |
    ARM_SPI_MSB_LSB |
    ARM_SPI_DATA_BITS(8),
    1000000
  );
}

/* CS */
void BME280_CS_0(void)
{
  HAL_GPIO_WritePin(BME280_CS_PORT, BME280_CS_PIN, GPIO_PIN_RESET);
}

void BME280_CS_1(void)
{
  HAL_GPIO_WritePin(BME280_CS_PORT, BME280_CS_PIN, GPIO_PIN_SET);
}

/* Callback busy */
static void busy(uint32_t event)
{
  uint32_t mask;
  mask = ARM_SPI_EVENT_TRANSFER_COMPLETE;

  if (event & mask) {
    osEventFlagsSet(busyEventFlag, 0x01);
  }
}

/* Escritura / lectura SPI */
void BME280_wr_reg(uint8_t reg, uint8_t value)
{
  spi_tx[0] = reg & 0x7F;   // write
  spi_tx[1] = value;

  BME280_CS_0();

  SPIdrv->Send(spi_tx, 2);

  osEventFlagsWait(busyEventFlag, 0x01, osFlagsWaitAny, osWaitForever);
  osEventFlagsClear(busyEventFlag, 0x01);

  BME280_CS_1();
}

void BME280_rd_regs(uint8_t reg, uint8_t *data, uint32_t len)
{
  uint32_t i;

  spi_tx[0] = reg | 0x80;   // read
  for (i = 1; i < len + 1; i++) {
    spi_tx[i] = 0x00;
  }

  BME280_CS_0();

  SPIdrv->Transfer(spi_tx, spi_rx, len + 1);

  osEventFlagsWait(busyEventFlag, 0x01, osFlagsWaitAny, osWaitForever);
  osEventFlagsClear(busyEventFlag, 0x01);

  BME280_CS_1();

  for (i = 0; i < len; i++) {
    data[i] = spi_rx[i + 1];
  }
}

/* Funciones BME280 básicas */
uint8_t BME280_ReadID(void)
{
  uint8_t data;
  BME280_rd_regs(BME280_REG_ID, &data, 1);
  return data;
}

uint8_t BME280_ReadStatus(void)
{
  uint8_t data;
  BME280_rd_regs(BME280_REG_STATUS, &data, 1);
  return data;
}

void BME280_Reset(void)
{
  BME280_wr_reg(BME280_REG_RESET, BME280_SOFT_RESET);
}

void BME280_Config(void)
{
  /* ctrl_hum: humedad x1 */
  BME280_wr_reg(BME280_REG_CTRL_HUM, 0x01);

  /* config: filtro off, spi3w off */
  BME280_wr_reg(BME280_REG_CONFIG, 0x00);

  /* ctrl_meas:
     osrs_t = x1
     osrs_p = skip
     mode   = sleep
  */
  BME280_wr_reg(BME280_REG_CTRL_MEAS, 0x20);
}

void BME280_TriggerMeasurement(void)
{
  /* osrs_t = x1, osrs_p = skip, forced mode */
  BME280_wr_reg(BME280_REG_CTRL_MEAS, 0x21);
}

/* Lectura de calibración */
void BME280_ReadCalibration(void)
{
  uint8_t buf1[26];
  uint8_t buf2[7];

  BME280_rd_regs(0x88, buf1, 26);
  BME280_rd_regs(0xE1, buf2, 7);

  bme_cal.dig_T1 = (uint16_t)((buf1[1] << 8) | buf1[0]);
  bme_cal.dig_T2 = (int16_t)((buf1[3] << 8) | buf1[2]);
  bme_cal.dig_T3 = (int16_t)((buf1[5] << 8) | buf1[4]);

  bme_cal.dig_H1 = buf1[25];
  bme_cal.dig_H2 = (int16_t)((buf2[1] << 8) | buf2[0]);
  bme_cal.dig_H3 = buf2[2];
  bme_cal.dig_H4 = (int16_t)((buf2[3] << 4) | (buf2[4] & 0x0F));
  bme_cal.dig_H5 = (int16_t)((buf2[5] << 4) | (buf2[4] >> 4));
  bme_cal.dig_H6 = (int8_t)buf2[6];
}

/* Lectura raw en burst */
void BME280_ReadRawData(int32_t *adc_T, int32_t *adc_H)
{
  uint8_t data[5];

  /* burst read desde temp_msb hasta hum_lsb */
  BME280_rd_regs(BME280_REG_TEMP_MSB, data, 5);

  *adc_T = ((int32_t)data[0] << 12) |
           ((int32_t)data[1] << 4)  |
           ((int32_t)data[2] >> 4);

  *adc_H = ((int32_t)data[3] << 8) |
           ((int32_t)data[4]);
}

/* Compensación temperatura
 * Devuelve 0.01 ºC */
int32_t BME280_Compensate_T(int32_t adc_T)
{
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)bme_cal.dig_T1 << 1))) *
           ((int32_t)bme_cal.dig_T2)) >> 11;

  var2 = (((((adc_T >> 4) - ((int32_t)bme_cal.dig_T1)) *
            ((adc_T >> 4) - ((int32_t)bme_cal.dig_T1))) >> 12) *
            ((int32_t)bme_cal.dig_T3)) >> 14;

  bme_cal.t_fine = var1 + var2;
  T = (bme_cal.t_fine * 5 + 128) >> 8;

  return T;
}

/* Compensación humedad
 * Devuelve %RH en float */
float BME280_Compensate_H(int32_t adc_H)
{
  float var;

  var = (float)bme_cal.t_fine - 76800.0f;

  var = (adc_H - (((float)bme_cal.dig_H4) * 64.0f +
        (((float)bme_cal.dig_H5) / 16384.0f) * var)) *
        (((float)bme_cal.dig_H2) / 65536.0f *
        (1.0f + (((float)bme_cal.dig_H6) / 67108864.0f) * var *
        (1.0f + (((float)bme_cal.dig_H3) / 67108864.0f) * var)));

  var = var * (1.0f - ((float)bme_cal.dig_H1) * var / 524288.0f);

  if (var > 100.0f) var = 100.0f;
  if (var < 0.0f)   var = 0.0f;

  return var;
}