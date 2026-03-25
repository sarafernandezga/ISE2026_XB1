#include "ThClk.h"
 
/*----------------------------------------------------------------------------
 
Thread 1 'Clock': Sample thread
---------------------------------------------------------------------------*/

osThreadId_t tid_Control_reloj;        // Thread Joystickid


Hora_t Hora; 

const osThreadAttr_t thread1_attr_clk = {
  .stack_size = 256                            // Create the thread stack with a size of 512 bytes
};

void ThControlReloj (void* argument);                   // Thread function

int Init_ThClk (void)
{
	static uint32_t exec;
	
  tid_Control_reloj = osThreadNew(ThControlReloj, NULL, &thread1_attr_clk);
	
  if (tid_Control_reloj == NULL)
  {
    return(-1);
  }
  return(0);
}

void ThControlReloj(void* args){
  Hora.Segundos = 0;
  Hora.Minutos = 0;
  Hora.Horas=0;
	
  while(1){

   if (Hora.Segundos == 60){
     Hora.Segundos = 0;
     Hora.Minutos++;
   }
   if (Hora.Minutos == 60){
     Hora.Minutos = 0;
     Hora.Horas++;
   }
	 if (Hora.Horas == 24){
		Hora.Horas=0;
	 }
	 
	 Hora.Segundos++;
	 
  osDelay(1000);
  }


}


