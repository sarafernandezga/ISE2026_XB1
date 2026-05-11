#include "PWM.h"
#include "BAJOCONSUMO.h"

osThreadId_t       tid_ThPWM;
osMessageQueueId_t pwm_Queue;

static const osThreadAttr_t attr_pwm = {
  .stack_size = 512
};

static TIM_HandleTypeDef htim1;

static void MX_TIM1_PWM_Init(void)
{
  __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};

  gpio.Pin       = PWM_GPIO_PIN;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = PWM_GPIO_AF;
  HAL_GPIO_Init(PWM_GPIO_PORT, &gpio);

  htim1.Instance = TIM1;
  htim1.Init.Prescaler         = 336 - 1;                 /* 168 MHz / 336 = 500 kHz */
  htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim1.Init.Period            = SERVO_PERIOD_TICKS - 1;  /* 20 ms */
  htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;

  HAL_TIM_PWM_Init(&htim1);

  TIM_OC_InitTypeDef sConfigOC = {0};

  sConfigOC.OCMode     = TIM_OCMODE_PWM1;
  sConfigOC.Pulse      = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, PWM_CHANNEL);
  HAL_TIM_PWM_Start(&htim1, PWM_CHANNEL);
}

/*
 * Convierte microsegundos a CCR.
 * Timer a 500 kHz -> 1 tick = 2 us.
 */
static inline uint32_t Servo_Us_To_CCR(uint16_t pulse_us)
{
  return ((uint32_t)pulse_us * SERVO_TIMER_CLK_HZ) / 1000000U;
}

/*
 * Convierte ángulo 0-180º a pulso 750-2250 us.
 *
 * 0º   -> 750 us
 * 90º  -> 1500 us
 * 180º -> 2250 us
 */
static uint16_t Servo_Angle_To_Us(uint8_t angle)
{
  uint32_t pulse_us;

  if (angle > SERVO_MAX_ANGLE) {
    angle = SERVO_MAX_ANGLE;
  }

  pulse_us = SERVO_MIN_US +
             (((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US)) / SERVO_MAX_ANGLE);

  return (uint16_t)pulse_us;
}

static void Servo_SetAngle(uint8_t angle)
{
  uint16_t pulse_us;
  uint32_t ccr;

  pulse_us = Servo_Angle_To_Us(angle);
  ccr = Servo_Us_To_CCR(pulse_us);

  __HAL_TIM_SET_COMPARE(&htim1, PWM_CHANNEL, ccr);
}

static void Servo_StopPulse(void)
{
  __HAL_TIM_SET_COMPARE(&htim1, PWM_CHANNEL, 0);
}

/*
 * Secuencia de dispensación:
 * 1. Abrir 90º
 * 2. Mantener abierto 5 segundos
 * 3. Cerrar
 * 4. Esperar nueva orden
 */

static void Servo_Dispensar(void)
{
  /*
   * Abrir compuerta 90º.
   */
  Servo_SetAngle(SERVO_POS_ABIERTO);

  /*
   * Mantener la compuerta abierta durante 5 segundos.
   * Durante este osDelay, el PWM sigue generándose por hardware.
   */
  osDelay(SERVO_TIEMPO_ABIERTO_MS);

  /*
   * Cerrar compuerta.
   */
  Servo_SetAngle(SERVO_POS_CERRADO);

  /*
   * Dar tiempo físico al servo para llegar a la posición cerrada.
   */
  osDelay(SERVO_TIEMPO_MOV_MS);

  /*
   * Paramos la señal para reducir zumbido/consumo.
   * Si necesitáis que el servo mantenga fuerza cerrando la compuerta,
   * comentad esta línea.
   */
  Servo_StopPulse();
}
static void ThPWM(void *argument)
{
  MSGQUEUE_PWM_t msg;

  (void)argument;

  Servo_SetAngle(SERVO_POS_CERRADO);
  osDelay(SERVO_TIEMPO_MOV_MS);
  Servo_StopPulse();

  while (1) {

    if (osMessageQueueGet(pwm_Queue, &msg, NULL, osWaitForever) == osOK) {

      LowPower_BusyEnter();

      switch (msg.cmd) {

        case PWM_CMD_SET_ANGLE:
          Servo_SetAngle(msg.angle);

          if (msg.hold_ms == 0U) {
            msg.hold_ms = 500U;
          }

          osDelay(msg.hold_ms);
          Servo_StopPulse();
          break;

        case PWM_CMD_DISPENSAR:
          Servo_Dispensar();
          break;

        default:
          break;
      }

      LowPower_BusyExit();
    }
  }
}

int Init_ThPWM(void)
{
  MX_TIM1_PWM_Init();

  pwm_Queue = osMessageQueueNew(MSGQUEUE_PWM_OBJECTS, sizeof(MSGQUEUE_PWM_t), NULL);
  if (pwm_Queue == NULL) {
    return -1;
  }

  tid_ThPWM = osThreadNew(ThPWM, NULL, &attr_pwm);
  if (tid_ThPWM == NULL) {
    return -1;
  }

  return 0;
}