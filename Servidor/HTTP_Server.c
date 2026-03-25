/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network
 * Copyright (c) 2004-2019 Arm Limited (or its affiliates). All rights reserved.
 *------------------------------------------------------------------------------
 * Name:    HTTP_Server.c
 * Purpose: HTTP Server example
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include "main.h"
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE
#include "lcd.h"
#include "pot.h"
#include "rtc.h"
#include "sntp.h"
#include "BAJOCONSUMO.h"
#include "cmsis_os2.h"
#include <string.h>
#include "stm32f4xx_hal.h"              // Keil::Device:STM32Cube HAL:Common
#include "Board_Buttons.h"              // ::Board Support:Buttons

// Main stack size must be multiple of 8 Bytes
#define APP_MAIN_STK_SZ (1024U)
uint64_t app_main_stk[APP_MAIN_STK_SZ / 8];
const osThreadAttr_t app_main_attr = {
  .stack_mem  = &app_main_stk[0],
  .stack_size = sizeof(app_main_stk)
};


extern osMessageQueueId_t lcd_Queue;

static __NO_RETURN void RTC_LCD_Thread(void *arg);

osThreadId_t tid_rtc_lcd;


extern uint8_t  get_button     (void);
extern void     netDHCP_Notify (uint32_t if_num, uint8_t option, const uint8_t *val, uint32_t len);

char lcd_text[2][20+1] = { "LCD line 1",
                           "LCD line 2" };


void LEDS_Init(void);
void Pot_init(void);
                           
__NO_RETURN void app_main (void *arg);


/* Read digital inputs */
uint8_t get_button (void) {
  return ((uint8_t)Buttons_GetState ());
}

/* IP address change notification */
void netDHCP_Notify (uint32_t if_num, uint8_t option, const uint8_t *val, uint32_t len) {

  (void)if_num;
  (void)val;
  (void)len;

//  if (option == NET_DHCP_OPTION_IP_ADDRESS) {
//    /* IP address change, trigger LCD update */
//    osThreadFlagsSet (TID_Display, 0x01);
//  }
}


/*----------------------------------------------------------------------------
  Main Thread 'main': Run Network
 *---------------------------------------------------------------------------*/
__NO_RETURN void app_main (void *arg) {
  (void)arg;
  
  Buttons_Initialize();
  
  LEDS_Init();
  Pot_init();
  RTC_Module_Init();
    
  netInitialize ();

  Init_ThLCD();
  Init_ThPot();
  
  tid_rtc_lcd = osThreadNew(RTC_LCD_Thread, NULL, NULL);
  SNTP_Init();
	
	LowPower_Init();
  
  osThreadExit();
}

void LEDS_Init(void){
  static GPIO_InitTypeDef led;
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  led.Mode = GPIO_MODE_OUTPUT_PP;
  led.Pull = GPIO_PULLUP;
  led.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  
  led.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOB, &led);
  
  led.Pin = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOB, &led);
  
  led.Pin = GPIO_PIN_14;
  HAL_GPIO_Init(GPIOB, &led);
  
  __HAL_RCC_GPIOD_CLK_ENABLE();                 // habilitacion del reloj del puerto B
  
  /* ********** CONFIGURACION LD1, LD2 Y LD3 ********* */
  led.Mode = GPIO_MODE_OUTPUT_PP;               // Modo salida Push-Pull
  led.Pull = GPIO_PULLUP;                       // Resistencia interna de Pull-Up
  led.Speed = GPIO_SPEED_FREQ_VERY_HIGH;        // Frecuencia very high
  
  led.Pin = GPIO_PIN_12;                         // LED VERDE (LD1)
  HAL_GPIO_Init(GPIOD, &led);
  
  led.Pin = GPIO_PIN_11;                         // LED AZUL (LD2)
  HAL_GPIO_Init(GPIOD, &led);
  
  led.Pin = GPIO_PIN_13;                        // LED ROJO (LD3)
  HAL_GPIO_Init(GPIOD, &led); 

  /* Apagados al inicio */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,  GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

}

void Pot_init(void){

	static GPIO_InitTypeDef Pot;
	
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	
	Pot.Mode = GPIO_MODE_ANALOG;
	Pot.Pull = GPIO_NOPULL;
	
	Pot.Pin = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOA, &Pot);
	
	Pot.Pin = GPIO_PIN_0;
	HAL_GPIO_Init(GPIOC, &Pot);
	
	
}


static __NO_RETURN void RTC_LCD_Thread(void *arg) {
  (void)arg;
  LCD_t msg;

  while (1) {
    snprintf(lcd_text[0], sizeof(lcd_text[0]), "%-20s", RTC_GetTimeString());
    snprintf(lcd_text[1], sizeof(lcd_text[1]), "%-20s", RTC_GetDateString());
    
    memset(&msg, 0, sizeof(msg));
    msg.linea = 1;
    snprintf(msg.mensaje, sizeof(msg.mensaje), "%s", RTC_GetTimeString());
    osMessageQueuePut(lcd_Queue, &msg, 0U, 0U);

    memset(&msg, 0, sizeof(msg));
    msg.linea = 2;
    snprintf(msg.mensaje, sizeof(msg.mensaje), "%s", RTC_GetDateString());
    osMessageQueuePut(lcd_Queue, &msg, 0U, 0U);

    osDelay(1000);
    
  }
}

