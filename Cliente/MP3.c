#include "MP3.h"
#include "com.h"
/*----------------------------------------------------------------------------
 
Thread 1 'MP3': Sample thread
---------------------------------------------------------------------------*/

osThreadId_t tid_Control_MP3_transmision;
osThreadId_t tid_Control_MP3_recepcion;

osMessageQueueId_t cola_entrada_MP3;
osMessageQueueId_t cola_salida_MP3;

extern osMessageQueueId_t cola_entrada;

void ThControlMP3Transmision (void* argument);
void ThControlMP3recepcion (void* args);

extern ARM_DRIVER_USART Driver_USART2;

void MP3_Callback(uint32_t event);

static ARM_DRIVER_USART * USARTdrv = &Driver_USART2;

static uint8_t error=0;

static const osThreadAttr_t thread2_attr = {
  .stack_size = 512                            // Create the thread stack with a size of 512 bytes
};


int Init_ThMP3 (void)
{
	
	tid_Control_MP3_transmision = osThreadNew(ThControlMP3Transmision, NULL, &thread2_attr);
	tid_Control_MP3_recepcion = osThreadNew(ThControlMP3Transmision, NULL, &thread2_attr);
	
	if (tid_Control_MP3_transmision == NULL)
  {
    return(-1);
  }
  return(0);
}


void ThControlMP3Transmision (void* argument){

	ARM_DRIVER_VERSION version;
	ARM_USART_CAPABILITIES drv_capabilities;
	
	USARTdrv->Initialize(MP3_Callback);
	USARTdrv->PowerControl(ARM_POWER_FULL);
	USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 9600);
	
	USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
	USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
	
	
	
	uint8_t buffer_datos_entrada[BUFFER_SIZEMP3];
	cola_entrada_MP3 = osMessageQueueNew(4, sizeof(buffer_datos_entrada), NULL);

	while(1){
		
		osMessageQueueGet(cola_entrada_MP3, &buffer_datos_entrada, NULL, osWaitForever);
		if(USARTdrv->Send(buffer_datos_entrada,10) != ARM_DRIVER_OK){
			error++;
		}; 
		
		osThreadFlagsWait(Flag_Recibido2, osFlagsWaitAny, osWaitForever);
		
	}

}

void ThControlMP3recepcion (void* args){
	char cmd;
	int index = 0;
	char buffer_datos_salida[BUFFER_SIZEMP3];
	cola_salida_MP3 = osMessageQueueNew(10, sizeof(buffer_datos_salida), NULL);
	EstadoMP3_t estado = InitStateMP3;
	
	while(1){
			if (USARTdrv->Receive(&cmd, 1) != ARM_DRIVER_OK) {
			error++;
		}
		osThreadFlagsWait(Flag_Recibido, osFlagsWaitAny, osWaitForever);
	switch(estado){
		
			case InitStateMP3:
				
			if(cmd == SOH){
				buffer_datos_salida[index]= cmd;
				index++;
				estado = DefaultStateMP3;
			}
			
			break;		
			
			case DefaultStateMP3: 
			
				if (cmd != EOT){
					if(index < BUFFER_SIZEMP3-2){
						buffer_datos_salida[index] = cmd;
						index++;
						estado = InitStateMP3;
					}else{
						error++;
						index=0;
						estado = InitStateMP3;
					}
				
				}else{
					
					if(buffer_datos_salida[2] == (index-1)){
					buffer_datos_salida[index]=EOT;
					buffer_datos_salida[index+1] = '\0';
					osMessageQueuePut(cola_salida_MP3, &buffer_datos_salida, NULL, 0U);
					index=0;
					
				}else{
				 //ERROR
					error++;
					index=0;
					estado = InitStateMP3;
				}
				}
	  }
  }
}	

void MP3_Callback(uint32_t event) {
    // Manejo de eventos del driver
	uint32_t mask2;
	mask2 = ARM_USART_EVENT_TX_COMPLETE;
	uint32_t mask1;
	mask1 = ARM_USART_EVENT_RECEIVE_COMPLETE;
	
		if (event & mask1) {
		osThreadFlagsSet(tid_Control_MP3_recepcion, Flag_Recibido);
	}
	if (event & mask2) {
		osThreadFlagsSet(tid_Control_MP3_transmision, Flag_Recibido2);
	}
}

