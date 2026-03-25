#include "leds_N.h"
 
/*----------------------------------------------------------------------------
 
Thread 1 'LEDS': Sample thread
---------------------------------------------------------------------------*/

osThreadId_t tid_Control_Led;								         // Thread Leds
void ThControlLed (void* argument);                  // Thread function led 1

osMessageQueueId_t RGB_Queue;
MSGQUEUE_RGB_t ctrl_rgb;

osTimerId_t tim_rgb;
void tim_rgb_callback(void* argument);

const osThreadAttr_t thread1_attr_leds = {
  .stack_size = 128                            // Create the thread stack with a size of 128 bytes
};



int Init_ThLEDS (void)
{	
	tid_Control_Led = osThreadNew(ThControlLed, NULL, &thread1_attr_leds);
	
	RGB_Queue = osMessageQueueNew(4, sizeof(ctrl_rgb), NULL);
	
	tim_rgb = osTimerNew(tim_rgb_callback, osTimerPeriodic, &ctrl_rgb, NULL);
	
  if (tid_Control_Led == NULL)
  {
    return(-1);
  }
	
  return(0);
}


void ThControlLed(void* args){

	uint32_t status;
	
  while(1){
		osMessageQueueGet(RGB_Queue, &ctrl_rgb, 0U, osWaitForever);
		
		if(ctrl_rgb.rgb == Leds_OFF ){
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
			osTimerStop(tim_rgb);
		}else{
			osTimerStart(tim_rgb, ctrl_rgb.freq);
		}
		
  }
}

void tim_rgb_callback(void* argument){
	MSGQUEUE_RGB_t* ctrl_rgb = (MSGQUEUE_RGB_t*) argument;

		if (ctrl_rgb->rgb == LedVerde_ON) {
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
    }

    if (ctrl_rgb->rgb == LedAzul_ON) {
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
    }

    if (ctrl_rgb->rgb == LedRojo_ON) {
			HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
    }
			
}

