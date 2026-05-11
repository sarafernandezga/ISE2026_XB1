#include "stm32f4xx_hal.h"
#include "adc.h"
#include "BAJOCONSUMO.h"

/* Hilo ADC */
void ThADC(void *argument);
osThreadId_t tid_ThADC;

/* Message Queue */
osMessageQueueId_t pot_Queue;

static const osThreadAttr_t thread_attr_adc = {
  .stack_size = 512
};

static inline uint16_t Pot_MapToPeso(uint32_t adc_val)
{
  float v_adc;
  float peso;

  if (adc_val > 4095U) {
    adc_val = 4095U;
  }

  v_adc = ((float)adc_val / ADC_RESOLUTION_12B) * ADC_VREF;
  peso = (v_adc / ADC_VREF) * MAX_WEIGHT_G;

  if (peso < 0.0f) {
    peso = 0.0f;
  }

  if (peso > MAX_WEIGHT_G) {
    peso = MAX_WEIGHT_G;
  }

  return (uint16_t)(peso + 0.5f);
}

static inline uint16_t Pot_MapToConsumo(uint32_t adc_val)
{
  float v_adc;
  float v_shunt;
  float corriente_x100;

  if (adc_val > 4095U) {
    adc_val = 4095U;
  }

  v_adc = ((float)adc_val / ADC_RESOLUTION_12B) * ADC_VREF;

  /*
   * MAX4080S:
   * Vout = Gain * Vshunt
   * Vshunt = Vout / Gain
   * I = Vshunt / Rshunt
   *
   * Se devuelve I * 100 para enviarlo luego escalado.
   * Ejemplo: 2.50 A -> 250.
   */
  v_shunt = v_adc / CURRENT_GAIN;
  corriente_x100 = (v_shunt / SHUNT_RESISTOR) * 100.0f;

  if (corriente_x100 < 0.0f) {
    corriente_x100 = 0.0f;
  }

  if (corriente_x100 > 65535.0f) {
    corriente_x100 = 65535.0f;
  }

  return (uint16_t)(corriente_x100 + 0.5f);
}

void ADC_RequestSample(void)
{
  if (tid_ThADC != NULL) {
    osThreadFlagsSet(tid_ThADC, ADC_EVT_SAMPLE);
  }
}

void ADC1_pins_F429ZI_config(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*
   * PC0 -> ADC1_IN10 consumo
   * PC3 -> ADC1_IN13 peso
   */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

int ADC_Init_Single_Conversion(ADC_HandleTypeDef *hadc, ADC_TypeDef *ADC_Instance)
{
  hadc->Instance = ADC_Instance;
  hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc->Init.Resolution = ADC_RESOLUTION_12B;
  hadc->Init.ScanConvMode = DISABLE;
  hadc->Init.ContinuousConvMode = DISABLE;
  hadc->Init.DiscontinuousConvMode = DISABLE;
  hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc->Init.NbrOfConversion = 1;
  hadc->Init.DMAContinuousRequests = DISABLE;
  hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;

  if (HAL_ADC_Init(hadc) != HAL_OK) {
    return -1;
  }

  return 0;
}

static uint32_t ADC_getValue(ADC_HandleTypeDef *hadc, uint32_t Channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t raw = 0U;

  sConfig.Channel = Channel;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;

  if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK) {
    return 0U;
  }

  if (HAL_ADC_Start(hadc) != HAL_OK) {
    return 0U;
  }

  if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK) {
    raw = HAL_ADC_GetValue(hadc);
  }

  HAL_ADC_Stop(hadc);

  return raw;
}

int Init_MsgQueueADC(void)
{
  pot_Queue = osMessageQueueNew(MSGQUEUE_ADC_OBJECTS, sizeof(MSGQUEUE_POT_t), NULL);

  if (pot_Queue == NULL) {
    return -1;
  }

  return 0;
}

int Init_ThPot(void)
{
  if (Init_MsgQueueADC() != 0) {
    return -1;
  }

  tid_ThADC = osThreadNew(ThADC, NULL, &thread_attr_adc);

  return (tid_ThADC == NULL) ? -1 : 0;
}

void ThADC(void *argument)
{
  ADC_HandleTypeDef adchandle;
  MSGQUEUE_POT_t value;
  uint32_t flags;

  (void)argument;

  ADC1_pins_F429ZI_config();
  ADC_Init_Single_Conversion(&adchandle, ADC1);

  while (1) {

    flags = osThreadFlagsWait(ADC_EVT_SAMPLE,
                              osFlagsWaitAny,
                              osWaitForever);

    if ((flags & osFlagsError) != 0U) {
      continue;
    }

    LowPower_BusyEnter();

    value.consumo = Pot_MapToConsumo(ADC_getValue(&adchandle, ADC_CHANNEL_10));
    value.peso    = Pot_MapToPeso(ADC_getValue(&adchandle, ADC_CHANNEL_13));

    osMessageQueueReset(pot_Queue);
    osMessageQueuePut(pot_Queue, &value, 0U, 0U);

    LowPower_BusyExit();
  }
}