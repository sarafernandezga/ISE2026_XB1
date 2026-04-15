#ifndef __SPIH_H
#define __SPIH_H

#include "cmsis_os2.h"
#include "Driver_SPI.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
  float cmd;   // 0 = medir
} MSGQUEUE_HUM_t;

extern osThreadId_t tid_ThHum;
extern osMessageQueueId_t hum_Queue;

int Init_ThHum (void);

uint8_t BME280_IsInitialized(void);
uint16_t BME280_GetHumidityRaw(void);
float BME280_GetHumidityPercent(void);
int32_t BME280_GetTemperatureCentiDeg(void);

/* Registros BME280 */
#define BME280_REG_ID         0xD0
#define BME280_REG_RESET      0xE0
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_STATUS     0xF3
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_TEMP_MSB   0xFA

#define BME280_CHIP_ID        0x60
#define BME280_SOFT_RESET     0xB6


#define BME280_CS_PORT        GPIOD
#define BME280_CS_PIN         GPIO_PIN_14

/* =========================
 * Calibración BME280
 * ========================= */
typedef struct {
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;

  uint8_t  dig_H1;
  int16_t  dig_H2;
  uint8_t  dig_H3;
  int16_t  dig_H4;
  int16_t  dig_H5;
  int8_t   dig_H6;

  int32_t  t_fine;
} BME280_CALIB_t;

static BME280_CALIB_t bme_cal;

/* =========================
 * Prototipos internos
 * ========================= */
void BME280_GPIO_Init(void);
void BME280_SPI_Init(void);

void BME280_CS_0(void);
void BME280_CS_1(void);

void BME280_wr_reg(uint8_t reg, uint8_t value);
void BME280_rd_regs(uint8_t reg, uint8_t *data, uint32_t len);

uint8_t BME280_ReadStatus(void);
uint8_t BME280_ReadID(void);
void BME280_Reset(void);
void BME280_Config(void);
void BME280_TriggerMeasurement(void);
void BME280_Init(void);

void BME280_ReadCalibration(void);
void BME280_ReadRawData(int32_t *adc_T, int32_t *adc_H);

int32_t BME280_Compensate_T(int32_t adc_T);
float   BME280_Compensate_H(int32_t adc_H);


#endif