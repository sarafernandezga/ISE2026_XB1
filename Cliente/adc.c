#include "adc.h"

//hilos
osThreadId_t            tid_ThPot;            
osMessageQueueId_t      pot_Queue;   

static const osThreadAttr_t thread_attr_pot = {
  .stack_size = 256
};

// ADC – Configuración (ADC1, canales 3 y 10)


static ADC_HandleTypeDef hadc1;

static void MX_ADC1_Init (void)
{
  __HAL_RCC_ADC1_CLK_ENABLE();

  hadc1.Instance                   = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4; /* 42 MHz / 4  */
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode          = ENABLE;                  /* dos canales  */
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion       = 2;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  HAL_ADC_Init(&hadc1);

  ADC_ChannelConfTypeDef sConfig;


  /* POT_2  -> PC0 -> ADC1_IN10 */
  sConfig.Channel      = ADC_CHANNEL_10;
  sConfig.Rank         = 2;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static inline uint8_t Pot_MapToTemp (uint32_t adc_val)
{
  return (uint8_t)(POT_MIN_TEMP + ((float)adc_val / POT_ADC_MAX) * (POT_MAX_TEMP - POT_MIN_TEMP));
}

static void ThPot (void *argument)
{

  uint32_t raw2;
  MSGQUEUE_POT_t msg;

  while(1) {
    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY); /* Conversión canal 2 */
    raw2 = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    //Escalado a volumen (0-30)
    msg.Vol = Pot_MapToTemp(raw2);

    osMessageQueuePut(pot_Queue, &msg, 0U, 0U);

    osDelay(100U); 
  }
}

int Init_ThPot (void)
{
  MX_ADC1_Init();

  pot_Queue = osMessageQueueNew(MSGQUEUE_POT_OBJECTS, sizeof(MSGQUEUE_POT_t), NULL);
  if (pot_Queue == NULL) {
    return -1;
  }

  tid_ThPot = osThreadNew(ThPot, NULL, &thread_attr_pot);
	
  return (tid_ThPot == NULL) ? -1 : 0;
}
