#include "BAJOCONSUMO.h"
#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "Board_Buttons.h"

static osThreadId_t tid_lowpower = NULL;
static volatile uint8_t sleep_requested = 0;
static volatile uint8_t sleeping = 0;

static void BlueButton_EXTI_Init(void);
static __NO_RETURN void LowPower_Thread(void *argument);
static __NO_RETURN void Blink_Thread(void *argument);

void LowPower_Init(void)
{
  BlueButton_EXTI_Init();

  osThreadNew(Blink_Thread, NULL, NULL);
  tid_lowpower = osThreadNew(LowPower_Thread, NULL, NULL);
}

void LowPower_RequestSleep(void)
{
  sleep_requested = 1;
}

uint8_t LowPower_IsSleeping(void)
{
  return sleeping;
}



static void BlueButton_EXTI_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  GPIO_InitStruct.Pin  = GPIO_PIN_13;              // USER button B1
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13) {
    /* no hace falta poner nada: la IRQ ya despierta al core del Sleep */
  }
}

static __NO_RETURN void Blink_Thread(void *argument)
{
  (void)argument;

  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    osDelay(100);
  }
}

static __NO_RETURN void LowPower_Thread(void *argument)
{
  (void)argument;

  osDelay(15000);   // 15 s desde el arranque

  sleep_requested = 1;

  while (1) {
    if (sleep_requested) {
      sleep_requested = 0;
      sleeping = 1;

      /* LED rojo encendido antes de entrar */
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);

      /* Pequeño retardo para asegurar que ves el LED rojo encendido */
      osDelay(20);

      /* Entrar en Sleep */
      HAL_SuspendTick();
      HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
      HAL_ResumeTick();

      /* Al salir de Sleep */
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
      sleeping = 0;
    }

    osDelay(50);
  }
}