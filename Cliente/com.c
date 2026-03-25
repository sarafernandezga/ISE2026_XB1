#include "COM.h"
 
/*----------------------------------------------------------------------------
 
Thread 1 'Acelerometro': Sample thread
---------------------------------------------------------------------------*/

osThreadId_t tid_Control_Com_recepcion;        // Thread Joystickid
osThreadId_t tid_Control_Com_transmision;

osMessageQueueId_t cola_entrada;
osMessageQueueId_t cola_salida;
void ThControlComRecepcion (void* argument);
void ThControlComTransmision (void* argument);

extern ARM_DRIVER_USART Driver_USART3;

void Com_Callback(uint32_t event);

static ARM_DRIVER_USART * USARTdrv = &Driver_USART3;

static uint8_t error=0;

static const osThreadAttr_t thread1_attr = {
  .stack_size = 256                            // Create the thread stack with a size of 512 bytes
};


int Init_ThCom (void)
{
	
  tid_Control_Com_recepcion = osThreadNew(ThControlComRecepcion, NULL, &thread1_attr); 
	tid_Control_Com_transmision = osThreadNew(ThControlComTransmision, NULL, &thread1_attr);
	
  if (tid_Control_Com_recepcion == NULL)
  {
    return(-1);
  }
	if (tid_Control_Com_transmision == NULL)
  {
    return(-1);
  }
  return(0);
}

void ThControlComRecepcion(void *argument) {

	//busyEventFlag = osEventFlagsNew(NULL);
	Estado_t estado = InitState;
	char bufferDatos[BUFFER_SIZE]; // strlen
	int index=0;
	cola_salida = osMessageQueueNew(4, sizeof(bufferDatos), NULL); 
	
	ARM_DRIVER_VERSION version;
	ARM_USART_CAPABILITIES drv_capabilities;
	char cmd;
	
	#ifdef DEBUG
	version = USARTdrv->GetVersion();
	if(version.api < 0x200){
		return;
	}
	drv_capabilities = USARTdrv->GetCapabilities();
	if(drv_capabilities.event_tx_complete == 0){
		return;
	}	
	#endif
	
	USARTdrv->Initialize(Com_Callback);
	USARTdrv->PowerControl(ARM_POWER_FULL);
	USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | ARM_USART_DATA_BITS_8 | ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 115200);
	
	USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
	USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
	
	while (1) {
		
		//USARTdrv->Receive(&cmd, 1);
		if (USARTdrv->Receive(&cmd, 1) != ARM_DRIVER_OK) {
			error++;
		}
		osThreadFlagsWait(Flag_Recibido, osFlagsWaitAny, osWaitForever);

		switch(estado){
		
			case InitState:
				
			if(cmd == SOH){
				bufferDatos[index]= cmd;
				index++;
				estado = DefaultState;
			}
			
			break;
			
			
			case DefaultState:
				
			if(cmd == EOT){
			
				if(bufferDatos[2] == (index-1)){
					
					bufferDatos[index]=EOT;
					bufferDatos[index+1] = '\0';
					//strncpy(bufferDatos,bufferDatos,bufferDatos[2]+1);
					osMessageQueuePut(cola_salida, &bufferDatos, NULL, 0U);
					index=0;
					
					
				}else{
				 //ERROR
					error++;
					index=0;
					estado = InitState;
				}
				estado = InitState;
			}else{
				if(index >= BUFFER_SIZE-2){
						error++;
						index=0;
						estado = InitState;
				}else{
						bufferDatos[index] = cmd;
						index++;
				}
			}
			
			break;
		
		}
			
	}
}

void ThControlComTransmision (void* argument){

	char buffer_datos_entrada[BUFFER_SIZE];
	cola_entrada = osMessageQueueNew(4, sizeof(buffer_datos_entrada), NULL);
	
	int i=0;
	while(1){
		
		osMessageQueueGet(cola_entrada, &buffer_datos_entrada, NULL, osWaitForever);
		USARTdrv->Send(buffer_datos_entrada, strlen(buffer_datos_entrada)); 
		osThreadFlagsWait(Flag_Recibido2, osFlagsWaitAny, osWaitForever);
		
	}

}



void Com_Callback(uint32_t event) {
    // Manejo de eventos del driver
	uint32_t mask;
	uint32_t mask2;
	mask = ARM_USART_EVENT_RECEIVE_COMPLETE;
	mask2 = ARM_USART_EVENT_SEND_COMPLETE;
	if (event & mask) {
		osThreadFlagsSet(tid_Control_Com_recepcion, Flag_Recibido);
	}
	if (event & mask2) {
		osThreadFlagsSet(tid_Control_Com_transmision, Flag_Recibido2);
	}
}
