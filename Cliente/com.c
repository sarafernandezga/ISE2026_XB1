#include "COM.h"
 
osThreadId_t tid_Control_Com_recepcion;
osThreadId_t tid_Control_Com_transmision;

osMessageQueueId_t cola_entrada;
osMessageQueueId_t cola_salida;

MSGQUEUE_Data_to_server_t Data_to_server;
MSGQUEUE_Data_to_client_t Data_to_client;
int ack;

void ThControlComRecepcion (void* argument);
void ThControlComTransmision (void* argument);

extern ARM_DRIVER_USART Driver_USART3;

void Com_Callback(uint32_t event);

static ARM_DRIVER_USART * USARTdrv = &Driver_USART3;

static uint8_t error=0;

static const osThreadAttr_t thread1_attr = {
  .stack_size = 256
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
	uint8_t bufferDatos[BUFFER_SIZE]; // strlen
	int index=0;
	cola_salida = osMessageQueueNew(4, sizeof(Data_to_client), NULL); 
	
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
					
				bufferDatos[index]=EOT;
				Data_to_client.dispensar = bufferDatos[1]; 
				Data_to_client.ack = bufferDatos[2];
				osMessageQueuePut(cola_salida, &Data_to_client, NULL, 0U);
				index=0;
				//mandamos ack
				ack = 1;
				estado = InitState;
			}else{
				if(index >= 3){
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
	cola_entrada = osMessageQueueNew(4, sizeof(Data_to_server), NULL);
	
	int i=0;
	while(1){
		
		osMessageQueueGet(cola_entrada, &Data_to_server, NULL, osWaitForever);
		buffer_datos_entrada[0] = SOH;
		buffer_datos_entrada[1] = Data_to_server.consumo;
		buffer_datos_entrada[2] = Data_to_server.Distancia;
		buffer_datos_entrada[3] = Data_to_server.humedad;
		buffer_datos_entrada[4] = Data_to_server.peso;
		buffer_datos_entrada[5] = Data_to_server.Estado;
		buffer_datos_entrada[6] = ack;
		buffer_datos_entrada[7] = EOT;		
		USARTdrv->Send(buffer_datos_entrada, strlen(buffer_datos_entrada)); 
		ack = 0;
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
