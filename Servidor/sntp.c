#include "sntp.h"
#include "rl_net.h"
#include "rtc.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "Board_Buttons.h"
#include <stdint.h>
#include <time.h>

osThreadId_t tid_sntp = NULL;

static void SyncRtcFromSntp(void);
static __NO_RETURN void SNTP_Thread(void *arg);

void SleepMode_Measure(void);


/* Sincroniza el RTC con el servidor SNTP */
static void SyncRtcFromSntp(void)
{
  uint32_t utc_seconds = 0;
  uint32_t frac = 0;

  time_t rawtime;
  struct tm *ts;

  if (netSNTPc_GetTimeX("time.google.com", &utc_seconds, &frac) == netOK) {

    rawtime = (time_t)utc_seconds;

    /* localtime sin TZ devuelve UTC */
    ts = localtime(&rawtime);

    if (ts != NULL) {

      int year  = ts->tm_year + 1900;
      int month = ts->tm_mon + 1;
      int day   = ts->tm_mday;
      int hour  = ts->tm_hour + 1;   

      /* horario de verano simplificado */
      if (month >= 4 && month <= 9) {
        hour += 1;
      }

      /* corrección si pasa de 24h */
      if (hour >= 24) {
        hour -= 24;
        day += 1;
      }

      RTC_SetDateTime(
        (uint8_t)(year - 2000),
        (uint8_t)month,
        (uint8_t)day,
        (uint8_t)hour,
        (uint8_t)ts->tm_min,
        (uint8_t)ts->tm_sec,
        RTC_WEEKDAY_MONDAY   /* no usamos weekday */
      );

      for (int i = 0; i < 20; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
        osDelay(100);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        osDelay(100);
      }
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
      
    }
  }
}


/* Forzar sincronización manual */
void SNTP_ForceSyncNow(void)
{
  SyncRtcFromSntp();
}


/* Hilo SNTP */
static __NO_RETURN void SNTP_Thread(void *arg)
{
  uint8_t last_button = 0;
  uint32_t cnt = 0;

  (void)arg;

  osDelay(5000);

  /* primera sincronización */
  SyncRtcFromSntp();
    
  while (1) {

    uint8_t b = (uint8_t)Buttons_GetState();

    /* botón azul ? RTC = 01/01/2000 */
    if ((b & 0x01U) && !(last_button & 0x01U)) {
      RTC_SetToDefault2000();
    }

    last_button = b;

    osDelay(100);
//    cnt += 100;

//    /* resincronizar cada 3 min */
//    if (cnt >= 180000U) {

//      cnt = 0;
//      SyncRtcFromSntp();
//    }
  }
}

void SleepMode_Measure(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* Disable USB Clock */
  __HAL_RCC_USB_OTG_FS_CLK_DISABLE();

  /* Disable Ethernet Clock */
  __HAL_RCC_ETH_CLK_DISABLE();

  /* Configure all GPIO as analog to reduce current consumption on non used IOs */
  /* Enable GPIOs clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct); 
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct); 
  HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

  /* Disable GPIOs clock */
  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOD_CLK_DISABLE();
  __HAL_RCC_GPIOE_CLK_DISABLE();
  __HAL_RCC_GPIOF_CLK_DISABLE();
  __HAL_RCC_GPIOG_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
  __HAL_RCC_GPIOI_CLK_DISABLE();
  __HAL_RCC_GPIOJ_CLK_DISABLE();
  __HAL_RCC_GPIOK_CLK_DISABLE();

  /* Configure user Button */


  /* Suspend Tick increment to prevent wakeup by Systick interrupt. 
     Otherwise the Systick interrupt will wake up the device within 1ms (HAL time base) */
  HAL_SuspendTick();

  /* Request to enter SLEEP mode */
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

  /* Resume Tick interrupt if disabled prior to sleep mode entry */
  HAL_ResumeTick();
  
  /* Exit Ethernet Phy from LowPower mode */
}

/* Inicialización del módulo */
void SNTP_Init(void)
{
  tid_sntp = osThreadNew(SNTP_Thread, NULL, NULL);
}
