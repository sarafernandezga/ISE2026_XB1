#include "Thjoystick.h"
 
/*  Salidas -> MsgQueue */
osMessageQueueId_t joy_Queue;

/*  Variables Thread Joystick */
osThreadId_t tid_ThJOYSTICK;                         // thread joystick id
int Init_ThJOYSTICK (void);                          // funcion que crea el hilo del joystick
void ThJOYSTICK (void *argument);                    // thread joystick function

/*  Variables Timers para gestionar rebotes y diferenciar entre pulsaciones largas y cortas  */
osTimerId_t tid_tim_rebotes;                         // timer id para gestionar los rebotes del joystick
osTimerId_t tid_tim_largo;                           // timer id para identificar una pulsacion larga
int Init_Timers(void);                               // funcion que crea e inicializa los timers virtuales que gestionan los rebotes y las pulsaciones cortas/largas
static void TimerRebotes_callback (void *argument);  // funcion callback asociada al timer que gestiona los rebotes
static void TimerLargo_callback (void *argument);    // funcion callback asociada al timer que diferencia pulsaciones cortas y largas

/* Variables globales */
uint8_t ciclos;                                      // contador de ciclos del timer para la pulsacion larga (Si llega a 20 ciclos o mas se tiene una pulsacion larga)


/*----------------------------------------------------------------------------
 *                         Joystick Initialize & ISR                         *
 *---------------------------------------------------------------------------*/
int Joystick_Init (void)
{
  static GPIO_InitTypeDef joystick;
  
  __HAL_RCC_GPIOB_CLK_ENABLE();                 // habilitacion del reloj del puerto B
  __HAL_RCC_GPIOE_CLK_ENABLE();                 // habilitacion del reloj del puerto E
  
  joystick.Mode = GPIO_MODE_IT_RISING;          // modo deteccion de interrupciones (flancos de subida)
  joystick.Pull = GPIO_PULLDOWN;                // resistencia interna de Pull-Down
  joystick.Speed = GPIO_SPEED_FREQ_VERY_HIGH;   // velocidad muy alta
  
  joystick.Pin = UP | RIGHT;                    // UP y RIGHT
  HAL_GPIO_Init(GPIOB, &joystick);              // inicializacion de los pines del puerto B
  
  joystick.Pin = DOWN | LEFT | CENTER;          // DOWN, LEFT y CENTER
  HAL_GPIO_Init(GPIOE, &joystick);              // inicializacion de los pines del puerto E
  
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);           // habilitacion de interrupciones externas
	
  int8_t status = 0;
	
	return status;
}

void EXTI15_10_IRQHandler (void)
{
  HAL_GPIO_EXTI_IRQHandler(UP | RIGHT | DOWN | LEFT | CENTER);
}

void HAL_GPIO_EXTI_Callback (uint16_t GPIO_Pin)
{
  osThreadFlagsSet(tid_ThJOYSTICK, PULSACION);    // se manda un flag al thread del joystick indicando que se ha detectado una pulsacion
}

/*----------------------------------------------------------------------------
 *                                  Timers                                   *
 *---------------------------------------------------------------------------*/
int Init_Timers(void)
{
  static uint32_t exec = 1U;
  static uint32_t exec2 = 1U;
  
  tid_tim_rebotes = osTimerNew((osTimerFunc_t)&TimerRebotes_callback, osTimerOnce, &exec, NULL);
  tid_tim_largo = osTimerNew((osTimerFunc_t)&TimerLargo_callback, osTimerPeriodic, &exec2, NULL);
  
  return(0);
}

static void TimerRebotes_callback (void *argument)
{
  MSGQUEUE_JOY_t JOYSTICK_msg;           // mensaje para enviar a la cola
  
  if(HAL_GPIO_ReadPin(GPIOB, UP) == GPIO_PIN_SET)            // UP
  {
    JOYSTICK_msg.gesto = UP_CORTA;       // pulsacion corta de UP
  }
  
  else if(HAL_GPIO_ReadPin(GPIOB, RIGHT) == GPIO_PIN_SET)    // RIGHT
  {
    JOYSTICK_msg.gesto = RIGHT_CORTA;    // pulsacion corta de RIGHT
  }
  
  else if(HAL_GPIO_ReadPin(GPIOE, DOWN) == GPIO_PIN_SET)     // DOWN
  {
    JOYSTICK_msg.gesto = DOWN_CORTA;     // pulsacion corta de DOWN
  }
  
  else if(HAL_GPIO_ReadPin(GPIOE, LEFT) == GPIO_PIN_SET)     // LEFT
  {
    JOYSTICK_msg.gesto = LEFT_CORTA;     // pulsacion corta de LEFT
  }
  
  else if(HAL_GPIO_ReadPin(GPIOE, CENTER) == GPIO_PIN_SET)   // CENTER
  {
    /* se lanza el timer encargado de diferenciar entre pulsaciones cortas y largas para que se empiecen a contar ciclos de 50 ms */
    osTimerStart(tid_tim_largo, 50U);
		return;
	}
  
  /* se introduce en la cola el mensaje */
  osMessageQueuePut(joy_Queue, &JOYSTICK_msg, 0U, 0U);
}

static void TimerLargo_callback (void *argument)
{
  MSGQUEUE_JOY_t JOYSTICK_msg;            // mensaje para enviar a la cola
  
  if(HAL_GPIO_ReadPin(GPIOE, CENTER) == GPIO_PIN_RESET)      // CENTER ya no pulsado
  {
    ciclos = 0;                           // se ha dejado de pulsar el gesto "center" del joystick -> se reseta el contador de ciclos
    JOYSTICK_msg.gesto = CENTER_CORTA;    // pulsacion corta de CENTER
    osTimerStop(tid_tim_largo);           // se detiene el timer largo
		osMessageQueuePut(joy_Queue, &JOYSTICK_msg, 0U, 0U);
  }
  
  else                                                       // CENTER sigue pulsado
  {
    ciclos++;                             // si se sigue pulsando el gesto "center" del joystick -> se incrementa el contador de ciclos
    
    if(ciclos >= 19)                      // si se han contado al menos 20 ciclos de 50 ms (1000 ms) --> Pulsacion larga
    {
      ciclos = 0;                         // se reseta el contador de ciclos
      JOYSTICK_msg.gesto = CENTER_LARGA;  // pulsacion larga de CENTER
      osTimerStop(tid_tim_largo);         // se detiene el timer largo
			osMessageQueuePut(joy_Queue, &JOYSTICK_msg, 0U, 0U);
    }
  }
  
  /* se introduce en la cola el mensaje */
 
}

/*----------------------------------------------------------------------------
 *                            Thread Joystick                                *
 *---------------------------------------------------------------------------*/
int Init_ThJOYSTICK (void)
{
	
  int8_t status = 0;
  /* se crea e inicia la cola de mensajes */
  joy_Queue = osMessageQueueNew(MSGQUEUE_JOYSTICK_OBJECTS, sizeof(MSGQUEUE_JOY_t), NULL);
  
	status |= Init_Timers();      // creacion de los temporizadores temporales de los rebotes y de las pulsaciones cortas/largas
  if (joy_Queue == NULL)
  {
    return (-1);
  }
  
  tid_ThJOYSTICK = osThreadNew(ThJOYSTICK, NULL, NULL);
  
  if (tid_ThJOYSTICK == NULL)
  {
    return(-1);
  }
  
  return(0);
}

void ThJOYSTICK (void *argument)
{
  ciclos = 0;     // Se inicializa la variable ciclos en cero
  
  while(1)
  {
    /* se espera a que se haya producido una pulsacion */
    osThreadFlagsWait(PULSACION, osFlagsWaitAll, osWaitForever);
 
    
    /* se inicia el timer que gestiona los rebotes */
    osTimerStart(tid_tim_rebotes, 50U);
  }
}