#include "Principal.h"
 
/*----------------------------------------------------------------------------
 
Thread 1 'Principal': Sample thread
---------------------------------------------------------------------------*/

osThreadId_t tid_Control_Principal;        // Thread Principal

//RGB variables

extern osMessageQueueId_t RGB_Queue;
MSGQUEUE_RGB_t msg_rgb;

//LM75

extern osMessageQueueId_t LM_Queue;       				//cola para recibir datos del lm75
       									//estructura para recibir datos del lm75

//COM-PC

extern osMessageQueueId_t cola_entrada;  					//cola para enviar datos al com-pc
extern osMessageQueueId_t cola_salida;   					//cola para recibir datos del com-pc
char buffer_datos_entrada_compc[BUFFER_SIZE];  		//buffer que tendrá los datos de entrada del com_pc
char buffer_datos_salida_compc[BUFFER_SIZE];   		//buffer que tendrá los datos de salida del com_pc

//MP3

extern osMessageQueueId_t cola_salida_MP3;
extern osMessageQueueId_t cola_entrada_MP3;
char buffer_datos_salida_mp3[BUFFER_SIZE];
char buffer_datos_entrada_mp3[BUFFER_SIZE];
MSG_MP3_t tramas_mp3;

//PWM
extern osMessageQueueId_t pwm_Queue;
MSGQUEUE_PWM_t duty_bip;

//ADC
extern osMessageQueueId_t pot_Queue;
MSGQUEUE_POT_t volumen;



osTimerId_t t_rep;
void t_rep_callback(void* argument);

void ThControlPrincipal (void* argument);         // Thread function

int Init_ThPrincipal (void)
{
	
  tid_Control_Principal = osThreadNew(ThControlPrincipal, NULL, NULL);
		
  if (tid_Control_Principal == NULL)
  {
    return(-1);
  }
  return(0);
}

void ThControlPrincipal(void* args){

	Estado_LCD modo_lcd = Reposo;
	static Hora_t Hora_aux;
	static uint8_t status;
	static uint8_t carpeta=0;
	static uint8_t fichero=0;
	static Hora_t tiempo;
	static bool sonando=false;
	static uint8_t carpeta_aux=0;
	static uint8_t fichero_aux=0;
	static char buffer_com[BUFFER_SIZE];
	Estado_programacion_t estado_programacion= HORAS;
	
	
	//RESET MP3
	tramas_mp3.accion4=0X0C;
	tramas_mp3.cantidad7=0X00;
	tramas_mp3.directory6=0x00;
	sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
	osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
	osMessageQueuePut(cola_entrada_MP3, &tramas_mp3, 0U, 0U);

	//SELECT DEVICE
	tramas_mp3.accion4=0X09;
	tramas_mp3.cantidad7=0X02;
	tramas_mp3.directory6=0x00;
	sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
	osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
	osMessageQueuePut(cola_entrada_MP3, &tramas_mp3, 0U, 0U);
	
	//SLEEP
	tramas_mp3.accion4=0X0A;
	tramas_mp3.cantidad7=0X00;
	tramas_mp3.directory6=0x00;
	sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
	osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
	osMessageQueuePut(cola_entrada_MP3, &tramas_mp3, 0U, 0U);	
	
	
	t_rep = osTimerNew(t_rep_callback, osTimerPeriodic, &tiempo, NULL);

	
  while(1){
		
		status = osMessageQueueGet(joy_Queue, &msg_recibir_joystick, NULL, 0U);				
		
		switch(modo_lcd){
			default:
				
			//---MODO REPOSO
			
			case Reposo:
				
				osMessageQueueGet(LM_Queue, &tempLM, NULL, 0U);
			
				if(msg_recibir_joystick.gesto == CENTER_LARGA){
					
					modo_lcd = Reproduccion;
					tramas_mp3.accion4=0X0B; //Despertar
					tramas_mp3.cantidad7=0X00;
					tramas_mp3.directory6=0x00;
					
					sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
					osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
					osMessageQueuePut(cola_entrada_MP3, &tramas_mp3, 0U, 0U);
				} 
				
				sprintf(msg_enviar_lcd.mensaje, "  SBM 2025 Temp:%.1f C", tempLM.Ti);
				msg_enviar_lcd.linea = 1;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
				
				sprintf(msg_enviar_lcd.mensaje, "       %02d:%02d:%02d", Hora.Horas, Hora.Minutos, Hora.Segundos);
				msg_enviar_lcd.linea = 2;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
				
			break;
				
				
				
				//---MODO REPRODUCCION---
			
			case Reproduccion:
				
				if (osMessageQueueGet(pot_Queue, &volumen, 0U, 0U) == osOK){
					tramas_mp3.accion4=0X06; //Volumen
					tramas_mp3.directory6=0X00;
					tramas_mp3.cantidad7=volumen.Vol;
					sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
					osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
				}				
				
				if(msg_recibir_joystick.gesto == CENTER_LARGA){
					modo_lcd = Programacion;
					Hora_aux.Horas = Hora.Horas;
					Hora_aux.Minutos = Hora.Minutos;
					Hora_aux.Segundos = Hora.Segundos;
					tramas_mp3.accion4=0X16; //STOP
					tramas_mp3.directory6=0X00;
					tramas_mp3.cantidad7=0X00;
					sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
					osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);

				}
				
				if(msg_recibir_joystick.gesto == CENTER_CORTA){
					
					duty_bip.duty=20;
					osMessageQueuePut(pwm_Queue, &duty_bip, 0U, 0U);
					
					if(sonando == true){
						sonando = false;
						tramas_mp3.accion4=0X0E; //pause
						tramas_mp3.directory6=0X00;
						tramas_mp3.cantidad7=0X00;	
						sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
						osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
						
						osTimerStop(t_rep);
						
						msg_rgb.freq = 1000;
						msg_rgb.rgb = LedAzul_ON;
						osMessageQueuePut(RGB_Queue, &msg_rgb, 0U, 0U);
					
					}else{
						sonando = true;
						osTimerStart(t_rep, 1000U);
						msg_rgb.freq = 250;
						msg_rgb.rgb = LedVerde_ON;
						osMessageQueuePut(RGB_Queue, &msg_rgb, 0U, 0U);
						
						if(carpeta_aux != carpeta || fichero_aux != fichero){
							tramas_mp3.accion4=0X0F; // play la cancion seleccionada
							tramas_mp3.directory6= fichero;
							tramas_mp3.cantidad7= carpeta;
							sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
							osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
							fichero_aux = fichero;
							carpeta_aux = carpeta;
							tiempo.Minutos=0;
							tiempo.Segundos=0;
						
						}else{
							tramas_mp3.accion4=0X0D; //play
							tramas_mp3.directory6=0X00;
							tramas_mp3.cantidad7=0X00;
							sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
							osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
							
						}
					}	
				}
				
				
				if(msg_recibir_joystick.gesto == RIGHT_CORTA){
					if (fichero == 255){
						fichero = 0;
					}else{
						fichero++;
					}
					
				}
				
				if(msg_recibir_joystick.gesto == LEFT_CORTA){
					if (fichero == 0){
						fichero = 255;
					}else{
						fichero--;
					}
				}
				
				if(msg_recibir_joystick.gesto == UP_CORTA){
					if (carpeta == 255){
						carpeta = 0;
					}else{
						carpeta++;

					}
				}
				
				if(msg_recibir_joystick.gesto == DOWN_CORTA){
					if (carpeta == 0){
						carpeta = 255;
					}else{
						carpeta--;

					}
				}
				
				sprintf(msg_enviar_lcd.mensaje, " F:%d C:%d,  VOL:%02d",carpeta, fichero, volumen.Vol);
				msg_enviar_lcd.linea = 1;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
				
				sprintf(msg_enviar_lcd.mensaje, "   T: %d:%d", tiempo.Minutos, tiempo.Segundos);
				msg_enviar_lcd.linea = 2;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
				
			
			break;
			
			
				
				//---MODO PROGRAMACION---
				
				
			case Programacion:
				osMessageQueueGet(LM_Queue, &tempLM, NULL, 0U);	
				
				switch(estado_programacion){
					
					default:
					case HORAS:
						if(msg_recibir_joystick.gesto == UP_CORTA){
							if(Hora_aux.Horas == 23){
								Hora_aux.Horas = 0;
							}else{
								Hora_aux.Horas++;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == DOWN_CORTA){
							if(Hora_aux.Horas == 0){
								Hora_aux.Horas = 23;
							}else{
								Hora_aux.Horas--;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == RIGHT_CORTA){
							estado_programacion = MINUTOS;
						}
						
						if(msg_recibir_joystick.gesto == LEFT_CORTA){
							estado_programacion = SEGUNDOS;
						}
						
					break;
					
					case MINUTOS:
						if(msg_recibir_joystick.gesto == UP_CORTA){
							if(Hora_aux.Minutos == 59){
								Hora_aux.Minutos = 0;
							}else{
								Hora_aux.Minutos++;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == DOWN_CORTA){
							if(Hora_aux.Minutos == 0){
								Hora_aux.Minutos = 59;
							}else{
								Hora_aux.Minutos--;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == RIGHT_CORTA){
							estado_programacion = SEGUNDOS;
						}
						
						if(msg_recibir_joystick.gesto == LEFT_CORTA){
							estado_programacion = HORAS;
						}
										
					break;
					
					
					case SEGUNDOS:
						if(msg_recibir_joystick.gesto == UP_CORTA){
							if(Hora_aux.Segundos == 59){
								Hora_aux.Segundos = 0;
							}else{
								Hora_aux.Segundos++;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == DOWN_CORTA){
							if(Hora_aux.Segundos == 0){
								Hora_aux.Segundos = 59;
							}else{
								Hora_aux.Segundos--;
							}
						
						}
						
						if(msg_recibir_joystick.gesto == RIGHT_CORTA){
							estado_programacion = HORAS;
						}
						
						if(msg_recibir_joystick.gesto == LEFT_CORTA){
							estado_programacion = MINUTOS;
						}
					
					break;
					
				}
					
				if(msg_recibir_joystick.gesto == CENTER_CORTA){
					Hora.Horas = Hora_aux.Horas;
					Hora.Minutos = Hora_aux.Minutos;
					Hora.Segundos = Hora_aux.Segundos;
				}
				if(msg_recibir_joystick.gesto == CENTER_LARGA){
					modo_lcd = Reposo;
					//SLEEP
					tramas_mp3.accion4=0X0A;
					tramas_mp3.cantidad7=0X00;
					tramas_mp3.directory6=0x00;
					sprintf(buffer_com, "%02d:%02d:%02d ---> 7E FF 06 %02X 00 %02X %02X EF \n", Hora.Horas, Hora.Minutos, Hora.Segundos, tramas_mp3.accion4, tramas_mp3.directory6, tramas_mp3.cantidad7);
					osMessageQueuePut(cola_entrada, &buffer_com, 0U, 0U);
					osMessageQueuePut(cola_entrada_MP3, &tramas_mp3, 0U, 0U);	
					
				}
				
				sprintf(msg_enviar_lcd.mensaje, "    HORA   Temp:%.1f C", tempLM.Ti);
				msg_enviar_lcd.linea = 1;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
				
				sprintf(msg_enviar_lcd.mensaje, "     %02d:%02d:%02d",Hora_aux.Horas, Hora_aux.Minutos, Hora_aux.Segundos);
				msg_enviar_lcd.linea = 2;
				osMessageQueuePut(lcd_Queue, &msg_enviar_lcd, 0U, 0U);
					
					
			}//switch
	}//while
				
		osDelay(50);
}//hilo


void t_rep_callback(void *args){

	Hora_t *tiempo=(Hora_t*) args;
	if(tiempo->Segundos==59){
		tiempo->Minutos++;
		tiempo->Segundos = 0;
	}
	if(tiempo->Minutos==59){
		tiempo->Minutos = 0;
	}
	tiempo->Segundos++;
}


