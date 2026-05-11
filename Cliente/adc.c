#include "stm32f4xx_hal.h"
#include "adc.h"

/* Hilo ADC */
void ThADC (void *argument);
osThreadId_t tid_ThADC; 

/* Message Queue */      
osMessageQueueId_t pot_Queue; 

MSGQUEUE_POT_t value;

static const osThreadAttr_t thread_attr_adc = {
  .stack_size = 256
};


/*
 * Convierte el valor bruto del ADC a peso en gramos.
 *
 * adc_val = 0      -> 0 g
 * adc_val = 4095   -> 1000 g
 *
 * Aunque el amplificador tenga ganancia 660 V/V, si ya has calibrado que
 * 3.3 V en la entrada del ADC equivalen a 1 kg, para la web interesa
 * mapear directamente el voltaje leído por el ADC a gramos.
 */
static inline float Pot_MapToPeso(uint32_t adc_val)
{
  float v_adc;

  if (adc_val > 4095U) {
    adc_val = 4095U;
  }

  v_adc = ((float)adc_val / ADC_RESOLUTION_12B) * ADC_VREF;

  return (v_adc / ADC_VREF) * MAX_WEIGHT_G;
}


/*
 * Convierte el valor bruto del ADC a potencia consumida en W.
 *
 * Pasos:
 * 1) ADC bruto -> tensión en el pin ADC
 * 2) Tensión ADC / ganancia del amplificador -> tensión real en la resistencia shunt
 * 3) I = Vshunt / Rshunt
 * 4) P = Vshunt * I = I^2 * Rshunt
 *
 * OJO: esta potencia es la disipada en la resistencia de medida.
 * Si quieres la potencia consumida por la carga completa, habría que usar:
 * P_carga = V_alimentacion_carga * I
 */
static inline float Pot_MapToConsumo(uint32_t adc_val)
{
  float v_adc;
  float v_shunt;
  float corriente;

  if (adc_val > 4095U) {
    adc_val = 4095U;
  }

  v_adc = ((float)adc_val / ADC_RESOLUTION_12B) * ADC_VREF;

  v_shunt = v_adc / CURRENT_GAIN;

  corriente = v_shunt / SHUNT_RESISTOR;

  return corriente*100;
}


/**
  * @brief config the use of analog inputs ADC123_IN10 and ADC123_IN13 and enable ADC1 clock
  * @param None
  * @retval None
  */
void ADC1_pins_F429ZI_config(){
	  GPIO_InitTypeDef GPIO_InitStruct = {0};
    
	__HAL_RCC_ADC1_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
    
  /*PC0     ------> ADC1_IN10
    PC3     ------> ADC1_IN13
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

}

/**
  * @brief Initialize the ADC to work with single conversions. 12 bits resolution, software start, 1 conversion
  * @param ADC handle
	* @param ADC instance
  * @retval HAL_StatusTypeDef HAL_ADC_Init
  */
int ADC_Init_Single_Conversion(ADC_HandleTypeDef *hadc, ADC_TypeDef  *ADC_Instance){
   /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc->Instance = ADC_Instance;
  hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc->Init.Resolution = ADC_RESOLUTION_12B;
  hadc->Init.ScanConvMode = ENABLE;
  hadc->Init.ContinuousConvMode = DISABLE;
  hadc->Init.DiscontinuousConvMode = DISABLE;
  hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc->Init.NbrOfConversion = 1;
  hadc->Init.DMAContinuousRequests = DISABLE;
 hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  
  if (HAL_ADC_Init(hadc) != HAL_OK){
    return -1;
  }
 
	 ADC_ChannelConfTypeDef sConfig = {0};

    /* Canal 1: ADC_CHANNEL_3 (PA3) */
    sConfig.Channel      = ADC_CHANNEL_3;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    HAL_ADC_ConfigChannel(hadc, &sConfig);
   
    /* Canal 2: ADC_CHANNEL_10 (PC0) */
    sConfig.Channel      = ADC_CHANNEL_10;
    sConfig.Rank         = 2;
    HAL_ADC_ConfigChannel(hadc, &sConfig);
   
	return 0;

}

/**
  * @brief Configure a specific channels ang gets the voltage in float type. This funtion calls to  HAL_ADC_PollForConversion that needs HAL_GetTick()
  * @param ADC_HandleTypeDef
	* @param channel number
	* @retval voltage in float (resolution 12 bits and VRFE 3.3
  */
float ADC_getValue(ADC_HandleTypeDef *hadc, uint32_t Channel){
		ADC_ChannelConfTypeDef sConfig = {0};
		HAL_StatusTypeDef status;

		uint32_t raw = 0;
		float voltage = 0;
		 /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = Channel;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK)
  {
    return -1;
  }
		
		HAL_ADC_Start(hadc);
		
		do {
			status = HAL_ADC_PollForConversion(hadc, 0); //This funtions uses the HAL_GetTick(), then it only can be executed wehn the OS is running
		}while(status != HAL_OK);
		
		raw = HAL_ADC_GetValue(hadc); 
		
		return raw;

}

/*----------------------------------------------------------------------------
 *      Message Queue ADC
 *---------------------------------------------------------------------------*/
int Init_MsgQueueADC (void){
  pot_Queue = osMessageQueueNew(MSGQUEUE_ADC_OBJECTS, sizeof(MSGQUEUE_POT_t), NULL);
  if (pot_Queue == NULL) {
    return -1;
  }
	
	return(0);
}

/*----------------------------------------------------------------------------
 *      Initialize ThADC
 *---------------------------------------------------------------------------*/

int Init_ThPot (void){
	Init_MsgQueueADC();
	
  tid_ThADC = osThreadNew(ThADC, NULL, &thread_attr_adc);
	
  return (tid_ThADC == NULL) ? -1 : 0;
}

/*----------------------------------------------------------------------------
 *      ThADC
 *---------------------------------------------------------------------------*/

void ThADC (void *argument) {
  
  // Inicializamos el ADC
  ADC_HandleTypeDef adchandle;                   //handler definition
	ADC1_pins_F429ZI_config();                     //specific PINS configuration
	ADC_Init_Single_Conversion(&adchandle , ADC1); //ADC1 configuration
  
  while (1) {
    // Obtenemos los valores del ADC de ambos potenciómetros
		uint32_t raw_consumo;
		uint32_t raw_peso;

		raw_consumo = (uint32_t)ADC_getValue(&adchandle, ADC_CHANNEL_10);
		raw_peso    = (uint32_t)ADC_getValue(&adchandle, ADC_CHANNEL_13);
	
		value.consumo = (uint8_t)Pot_MapToConsumo(raw_consumo);
		value.peso    = Pot_MapToPeso(raw_peso);
    // Mandamos los valores del ADC
    osMessageQueuePut(pot_Queue, &value, 0U, 0U);
    
		osDelay(100U);
   
  }
}
