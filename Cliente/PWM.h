#ifndef __PWM_H
#define __PWM_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

#define PWM_TIMER               TIM1
#define PWM_CHANNEL             TIM_CHANNEL_1
#define PWM_GPIO_PORT           GPIOE
#define PWM_GPIO_PIN            GPIO_PIN_9
#define PWM_GPIO_AF             GPIO_AF1_TIM1

/*
 * Timer:
 * 168 MHz / 336 = 500 kHz
 * 1 tick = 2 us
 * 10000 ticks = 20 ms = 50 Hz
 */
#define SERVO_TIMER_CLK_HZ      500000U
#define SERVO_PERIOD_TICKS      10000U

/*
 * Datasheet Parallax Standard Servo:
 * 0.75 ms - 2.25 ms cada 20 ms.
 */
#define SERVO_MIN_US            750U
#define SERVO_CENTER_US         1500U
#define SERVO_MAX_US            2250U

#define SERVO_MIN_ANGLE         0U
#define SERVO_MAX_ANGLE         180U

/*
 * Compuerta:
 * cerrado = 0 grados
 * abierto = 90 grados
 */
#define SERVO_POS_CERRADO       0U
#define SERVO_POS_ABIERTO       180U

#define SERVO_TIEMPO_MOV_MS     800U
#define SERVO_TIEMPO_ABIERTO_MS 5000U

#define MSGQUEUE_PWM_OBJECTS    4

typedef enum {
  PWM_CMD_SET_ANGLE = 0,
  PWM_CMD_DISPENSAR = 1
} PWM_CMD_t;

typedef struct {
  PWM_CMD_t cmd;
  uint8_t angle;
  uint16_t hold_ms;
} MSGQUEUE_PWM_t;

extern osMessageQueueId_t pwm_Queue;

int Init_ThPWM(void);

#endif