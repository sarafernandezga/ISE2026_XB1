#include "BAJOCONSUMO.h"
#include "Principal.h"

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/*----------------------------------------------------------
 * Configuración
 *---------------------------------------------------------*/

#define LOWPOWER_RTC_PERIOD_S       60U
#define LOWPOWER_SUSPEND_TICK       1U
#define LOWPOWER_DEBUG_LED          0U

/*----------------------------------------------------------
 * Variables internas
 *---------------------------------------------------------*/

static RTC_HandleTypeDef hrtc_lp;
static osThreadId_t tid_lowpower = NULL;

static volatile uint32_t busy_count = 0U;
static volatile uint8_t sleeping = 0U;

/*----------------------------------------------------------
 * Prototipos
 *---------------------------------------------------------*/

static int RTC_LowPower_Init(void);
static void LowPower_Thread(void *argument);

/*----------------------------------------------------------
 * API pública
 *---------------------------------------------------------*/

int LowPower_Init(void)
{
  if (RTC_LowPower_Init() != 0) {
    return -1;
  }

  tid_lowpower = osThreadNew(LowPower_Thread, NULL, NULL);

  if (tid_lowpower == NULL) {
    return -1;
  }

  return 0;
}

void LowPower_BusyEnter(void)
{
  __disable_irq();
  busy_count++;
  __enable_irq();
}

void LowPower_BusyExit(void)
{
  __disable_irq();

  if (busy_count > 0U) {
    busy_count--;
  }

  __enable_irq();
}

uint8_t LowPower_IsSleeping(void)
{
  return sleeping;
}

/*----------------------------------------------------------
 * Inicialización RTC WakeUp cada minuto
 *---------------------------------------------------------*/

static int RTC_LowPower_Init(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  /*
   * Intentamos usar LSE. Si no arranca, usamos LSI.
   * En algunas Nucleo puede no estar montado el cristal de 32.768 kHz.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK) {

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
      return -1;
    }

    hrtc_lp.Init.AsynchPrediv = 127U;
    hrtc_lp.Init.SynchPrediv = 255U;

  } else {

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      return -1;
    }

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
      return -1;
    }

    /*
     * LSI aproximado a 32 kHz:
     * 32000 / (128 * 250) = 1 Hz
     */
    hrtc_lp.Init.AsynchPrediv = 127U;
    hrtc_lp.Init.SynchPrediv = 249U;
  }

  __HAL_RCC_RTC_ENABLE();

  hrtc_lp.Instance = RTC;
  hrtc_lp.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc_lp.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc_lp.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc_lp.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

  if (HAL_RTC_Init(&hrtc_lp) != HAL_OK) {
    return -1;
  }

  HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);

  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc_lp);

  /*
   * RTC_WAKEUPCLOCK_CK_SPRE_16BITS usa el reloj de 1 Hz.
   * Periodo = WakeUpCounter + 1.
   * Para 60 s: 59.
   */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc_lp,
                                  LOWPOWER_RTC_PERIOD_S - 1U,
                                  RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
    return -1;
  }

  return 0;
}

/*----------------------------------------------------------
 * IRQ RTC WakeUp
 *---------------------------------------------------------*/

void RTC_WKUP_IRQHandler(void)
{
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc_lp);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
  if (hrtc->Instance == RTC) {
    Principal_NotifyEvent(PRINCIPAL_EVT_RTC);
  }
}

/*----------------------------------------------------------
 * Hilo bajo consumo
 *---------------------------------------------------------*/

static void LowPower_Thread(void *argument)
{
  (void)argument;

  osDelay(1000U);

  while (1) {

    if (busy_count == 0U) {

      sleeping = 1U;

#if LOWPOWER_DEBUG_LED
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
#endif

#if LOWPOWER_SUSPEND_TICK
      HAL_SuspendTick();
#endif

      HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

#if LOWPOWER_SUSPEND_TICK
      HAL_ResumeTick();
#endif

#if LOWPOWER_DEBUG_LED
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
#endif

      sleeping = 0U;
    }

    osDelay(20U);
  }
}