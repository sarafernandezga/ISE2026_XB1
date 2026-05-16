/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network:Service
 * Copyright (c) 2004-2018 ARM Germany GmbH. All rights reserved.
 *------------------------------------------------------------------------------
 * Name:    HTTP_Server_CGI.c
 * Purpose: HTTP Server CGI Module
 * Rev.:    V6.0.0
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cmsis_os2.h"                  // ::CMSIS:RTOS2
#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE
#include "main.h"
#include "lcd.h"
#include "rtc.h"
#include "PrincipalServidor.h"

#include "Board_LED.h"                  // ::Board Support:LED

#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif

// http_server.c
//extern uint16_t AD_in (uint32_t ch);
extern uint8_t  get_button (void);

//extern bool LEDrun;
extern char lcd_text[2][20+1];
//extern osThreadId_t TID_Display;

extern osMessageQueueId_t lcd_Queue;

LCD_t mensaje_lcd;

extern osMessageQueueId_t      pot_Queue;


// Local variables.
static uint8_t P2;
static uint8_t ip_addr[NET_ADDR_IP6_LEN];
static char    ip_string[40];

// My structure of CGI status variable.
typedef struct {
  uint8_t idx;
  uint8_t unused[3];
} MY_BUF;
#define MYBUF(p)        ((MY_BUF *)p)


// Process query string received by GET request.
void netCGI_ProcessQuery (const char *qstr) {
  netIF_Option opt = netIF_OptionMAC_Address;
  int16_t      typ = 0;
  char var[40];

  do {
    // Loop through all the parameters
    qstr = netCGI_GetEnvVar (qstr, var, sizeof (var));
    // Check return string, 'qstr' now points to the next parameter

    switch (var[0]) {
      case 'i': // Local IP address
        if (var[1] == '4') { opt = netIF_OptionIP4_Address;       }
        else               { opt = netIF_OptionIP6_StaticAddress; }
        break;

      case 'm': // Local network mask
        if (var[1] == '4') { opt = netIF_OptionIP4_SubnetMask; }
        break;

      case 'g': // Default gateway IP address
        if (var[1] == '4') { opt = netIF_OptionIP6_DefaultGateway; }
        else               { opt = netIF_OptionIP6_DefaultGateway; }
        break;

      case 'p': // Primary DNS server IP address
        if (var[1] == '4') { opt = netIF_OptionIP4_PrimaryDNS; }
        else               { opt = netIF_OptionIP6_PrimaryDNS; }
        break;

      case 's': // Secondary DNS server IP address
        if (var[1] == '4') { opt = netIF_OptionIP4_SecondaryDNS; }
        else               { opt = netIF_OptionIP6_SecondaryDNS; }
        break;
      
      default: var[0] = '\0'; break;
    }

    switch (var[1]) {
      case '4': typ = NET_ADDR_IP4; break;
      case '6': typ = NET_ADDR_IP6; break;

      default: var[0] = '\0'; break;
    }

    if ((var[0] != '\0') && (var[2] == '=')) {
      netIP_aton (&var[3], typ, ip_addr);
      // Set required option
      netIF_SetOption (NET_IF_CLASS_ETH, opt, ip_addr, sizeof(ip_addr));
    }
  } while (qstr);
}



static int ParseHoraMinuto(const char *str, uint8_t *h, uint8_t *m)
{
  int hh = 0;
  int mm = 0;

  if (str == NULL || h == NULL || m == NULL) {
    return 0;
  }

  if (sscanf(str, "%d:%d", &hh, &mm) == 2) {
    if ((hh >= 0) && (hh < 24) && (mm >= 0) && (mm < 60)) {
      *h = (uint8_t)hh;
      *m = (uint8_t)mm;
      return 1;
    }
  }

  return 0;
}



// Process data received by POST request.
// Type code: - 0 = www-url-encoded form data.
//            - 1 = filename for file upload (null-terminated string).
//            - 2 = file upload raw data.
//            - 3 = end of file upload (file close requested).
//            - 4 = any XML encoded POST data (single or last stream).
//            - 5 = the same as 4, but with more XML data to follow.
void netCGI_ProcessData (uint8_t code, const char *data, uint32_t len) {
  char var[40],passw[12];
	uint8_t auto_on = 0U;
	uint8_t guardar_config = 0U;
	uint8_t borrar_logs = 0U;
	uint8_t dispensar_manual = 0U;
	
	uint8_t h1_new = 0U;
	uint8_t m1_new = 0U;
	uint8_t h2_new = 0U;
	uint8_t m2_new = 0U;

	uint8_t h1_ok = 0U;
	uint8_t h2_ok = 0U;
	
  if (code != 0) {
    // Ignore all other codes
    return;
  }
	

  P2 = 0;
//  LEDrun = true;
  if (len == 0) {
    // No data or all items (radio, checkbox) are off
    LED_SetOut (P2);
    return;
  }
  passw[0] = 1;
  
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
  
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
  do {
    // Parse all parameters
    data = netCGI_GetEnvVar (data, var, sizeof (var));
    if (var[0] != 0) {
      // First character is non-null, string exists
      if (strcmp (var, "led0=on") == 0) {
        P2 |= 0x01;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      }
      else if (strcmp (var, "led1=on") == 0) {
        P2 |= 0x02;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
      }
      else if (strcmp (var, "led2=on") == 0) {
        P2 |= 0x04;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
      }
      else if (strcmp (var, "led3=on") == 0) {
        P2 |= 0x08;
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
      }
      else if (strcmp (var, "led4=on") == 0) {
        P2 |= 0x10;
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
      }
      else if (strcmp (var, "led5=on") == 0) {
        P2 |= 0x20;
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
      }
      else if (strcmp (var, "led6=on") == 0) {
        P2 |= 0x40;
      }
      else if (strcmp (var, "led7=on") == 0) {
        P2 |= 0x80;
      }
      else if (strcmp (var, "ctrl=Browser") == 0) {
//        LEDrun = false;
      }
      else if ((strncmp (var, "pw0=", 4) == 0) ||
               (strncmp (var, "pw2=", 4) == 0)) {
        // Change password, retyped password
        if (netHTTPs_LoginActive()) {
          if (passw[0] == 1) {
            strcpy (passw, var+4);
          }
          else if (strcmp (passw, var+4) == 0) {
            // Both strings are equal, change the password
            netHTTPs_SetPassword (passw);
          }
        }
      }
      else if (strncmp (var, "lcd1=", 5) == 0) {
        // LCD Module line 1 text
//        mensaje_lcd.linea=1;
//        strcpy (mensaje_lcd.mensaje, var+5);
//        osMessageQueuePut(lcd_Queue, &mensaje_lcd, 0U, 0U);
        
//        osThreadFlagsSet (TID_Display, 0x01);
      }
      else if (strncmp (var, "lcd2=", 5) == 0) {
        // LCD Module line 2 text
//        mensaje_lcd.linea=2;
//        strcpy (mensaje_lcd.mensaje, var+5);
//        osMessageQueuePut(lcd_Queue, &mensaje_lcd, 0U, 0U);
//        osThreadFlagsSet (TID_Display, 0x01);
            }
      else if (strncmp(var, "dispensar=", 10) == 0) {
        dispensar_manual = 1U;
      }/////COMEDERO
      else if (strcmp(var, "auto=on") == 0) {
        auto_on = 1U;
      }
      else if (strncmp(var, "guardar=", 8) == 0) {
        guardar_config = 1U;
      }
      else if (strncmp(var, "borrar_logs=", 12) == 0) {
        borrar_logs = 1U;
      }
      else if (strncmp(var, "hora1=", 6) == 0) {
        if (ParseHoraMinuto(var + 6, &h1_new, &m1_new)) {
          h1_ok = 1U;
        }
      }
      else if (strncmp(var, "hora2=", 6) == 0) {
        if (ParseHoraMinuto(var + 6, &h2_new, &m2_new)) {
          h2_ok = 1U;
        }
      }
    }
  } while (data);
	
	if (guardar_config != 0U) {
    PrincipalServidor_SetAuto(auto_on);

    if (h1_ok != 0U) {
      PrincipalServidor_SetHora1(h1_new, m1_new);
    }

    if (h2_ok != 0U) {
      PrincipalServidor_SetHora2(h2_new, m2_new);
    }
  }

  if (dispensar_manual != 0U) {
    PrincipalServidor_DispensarManual();
  }

  if (borrar_logs != 0U) {
    PrincipalServidor_LogClear();
  }
}

// Generate dynamic web data from a script line.
uint32_t netCGI_Script (const char *env, char *buf, uint32_t buflen, uint32_t *pcgi) {
  int32_t socket;
  netTCP_State state;
  NET_ADDR r_client;
  const char *lang;
  uint32_t len = 0U;
  uint8_t id;
  netIF_Option opt = netIF_OptionMAC_Address;
  int16_t      typ = 0;

  switch (env[0]) {
    // Analyze a 'c' script line starting position 2
    case 'a' :
      // Network parameters from 'network.cgi'
      switch (env[3]) {
        case '4': typ = NET_ADDR_IP4; break;
        case '6': typ = NET_ADDR_IP6; break;

        default: return (0);
      }
      
      switch (env[2]) {
        case 'l':
          // Link-local address
          if (env[3] == '4') { return (0);                             }
          else               { opt = netIF_OptionIP6_LinkLocalAddress; }
          break;

        case 'i':
          // Write local IP address (IPv4 or IPv6)
          if (env[3] == '4') { opt = netIF_OptionIP4_Address;       }
          else               { opt = netIF_OptionIP6_StaticAddress; }
          break;

        case 'm':
          // Write local network mask
          if (env[3] == '4') { opt = netIF_OptionIP4_SubnetMask; }
          else               { return (0);                       }
          break;

        case 'g':
          // Write default gateway IP address
          if (env[3] == '4') { opt = netIF_OptionIP4_DefaultGateway; }
          else               { opt = netIF_OptionIP6_DefaultGateway; }
          break;

        case 'p':
          // Write primary DNS server IP address
          if (env[3] == '4') { opt = netIF_OptionIP4_PrimaryDNS; }
          else               { opt = netIF_OptionIP6_PrimaryDNS; }
          break;

        case 's':
          // Write secondary DNS server IP address
          if (env[3] == '4') { opt = netIF_OptionIP4_SecondaryDNS; }
          else               { opt = netIF_OptionIP6_SecondaryDNS; }
          break;
		    
      }

      netIF_GetOption (NET_IF_CLASS_ETH, opt, ip_addr, sizeof(ip_addr));
      netIP_ntoa (typ, ip_addr, ip_string, sizeof(ip_string));
      len = (uint32_t)sprintf (buf, &env[5], ip_string);
      break;

    case 'b':
      // LED control from 'led.cgi'
      if (env[2] == 'c') {
        // Select Control
//        len = (uint32_t)sprintf (buf, &env[4], LEDrun ?     ""     : "selected",
//                                               LEDrun ? "selected" :    ""     );
        break;
      }
      // LED CheckBoxes
      id = env[2] - '0';
      if (id > 7) {
        id = 0;
      }
      id = (uint8_t)(1U << id);
      len = (uint32_t)sprintf (buf, &env[4], (P2 & id) ? "checked" : "");
      break;

    case 'c':
      // TCP status from 'tcp.cgi'
      while ((uint32_t)(len + 150) < buflen) {
        socket = ++MYBUF(pcgi)->idx;
        state  = netTCP_GetState (socket);

        if (state == netTCP_StateINVALID) {
          /* Invalid socket, we are done */
          return ((uint32_t)len);
        }

        // 'sprintf' format string is defined here
        len += (uint32_t)sprintf (buf+len,   "<tr align=\"center\">");
        if (state <= netTCP_StateCLOSED) {
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>-</td><td>-</td>"
                                             "<td>-</td><td>-</td></tr>\r\n",
                                             socket,
                                             netTCP_StateCLOSED);
        }
        else if (state == netTCP_StateLISTEN) {
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>%d</td><td>-</td>"
                                             "<td>-</td><td>-</td></tr>\r\n",
                                             socket,
                                             netTCP_StateLISTEN,
                                             netTCP_GetLocalPort(socket));
        }
        else {
          netTCP_GetPeer (socket, &r_client, sizeof(r_client));

          netIP_ntoa (r_client.addr_type, r_client.addr, ip_string, sizeof (ip_string));
          
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>%d</td>"
                                             "<td>%d</td><td>%s</td><td>%d</td></tr>\r\n",
                                             socket, netTCP_StateLISTEN, netTCP_GetLocalPort(socket),
                                             netTCP_GetTimer(socket), ip_string, r_client.port);
        }
      }
      /* More sockets to go, set a repeat flag */
      len |= (1u << 31);
      break;

    case 'd':
      // System password from 'system.cgi'
      switch (env[2]) {
        case '1':
          len = (uint32_t)sprintf (buf, &env[4], netHTTPs_LoginActive() ? "Enabled" : "Disabled");
          break;
        case '2':
          len = (uint32_t)sprintf (buf, &env[4], netHTTPs_GetPassword());
          break;
      }
      break;

    case 'e':
      // Browser Language from 'language.cgi'
      lang = netHTTPs_GetLanguage();
      if      (strncmp (lang, "en", 2) == 0) {
        lang = "English";
      }
      else if (strncmp (lang, "de", 2) == 0) {
        lang = "German";
      }
      else if (strncmp (lang, "fr", 2) == 0) {
        lang = "French";
      }
      else if (strncmp (lang, "sl", 2) == 0) {
        lang = "Slovene";
      }
      else {
        lang = "Unknown";
      }
      len = (uint32_t)sprintf (buf, &env[2], lang, netHTTPs_GetLanguage());
      break;

    case 'f':
      // LCD Module control from 'lcd.cgi'
      switch (env[2]) {
        case '1':
          len = (uint32_t)sprintf (buf, &env[4], lcd_text[0]);
          break;
        case '2':
          len = (uint32_t)sprintf (buf, &env[4], lcd_text[1]);
          break;
      }
      break;

    case 'g':
      // AD Input from 'ad.cgi'
      break;

    case 'x':
      // AD Input from 'ad.cgx'

      break;

    case 'y':
      // Button state from 'button.cgx'
      len = (uint32_t)sprintf (buf, "<checkbox><id>button%c</id><on>%s</on></checkbox>",
                               env[1], (get_button () & (1 << (env[1]-'0'))) ? "true" : "false");
      break;
      
    case 'h':
      // RTC from rtc.cgi / rtc.cgx
      switch (env[2]) {
        case '1':
          len = (uint32_t)sprintf(buf, &env[4], RTC_GetTimeString());
          break;
        case '2':
          len = (uint32_t)sprintf(buf, &env[4], RTC_GetDateString());
          break;
      }
      break;
		case 'z'://///////////COMEDERO///////////////
				{
					SERVIDOR_DATOS_t datos;
					SERVIDOR_LOG_t log;
					uint8_t h1, m1, h2, m2;
					char tmp[64];

					datos = PrincipalServidor_GetDatos();

					switch (env[2]) {

						case '1':
							len = (uint32_t)sprintf(buf, &env[4], RTC_GetTimeString());
							break;

						case '2':
							len = (uint32_t)sprintf(buf, &env[4], RTC_GetDateString());
							break;

						case '3':
							/*
							* El cliente manda peso / 10.
							* Por tanto, recuperamos gramos multiplicando por 10.
							*/
							snprintf(tmp, sizeof(tmp), "%u g", (unsigned int)datos.peso * 10U);
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;

						case '4':
							snprintf(tmp, sizeof(tmp), "%u %%", (unsigned int)datos.humedad);
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;

						case '5':
							if (datos.distancia == 255U || datos.distancia <=2) {
								snprintf(tmp, sizeof(tmp), "Deposito con comida ");
							} else {
								snprintf(tmp, sizeof(tmp), "Deposito vacio (%u cm)", (unsigned int)datos.distancia);
							}
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;

						case '6':
							snprintf(tmp, sizeof(tmp), "%u mA", (unsigned int)datos.consumo);
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;

						case '7':
							len = (uint32_t)sprintf(buf, &env[4], PrincipalServidor_GetEstadoTexto());
							break;

						case '8':
							len = (uint32_t)sprintf(buf, &env[4], PrincipalServidor_GetAlertaTexto());
							break;

						case '9':
							len = (uint32_t)sprintf(buf, &env[4], PrincipalServidor_GetAuto() ? "checked" : "");
							break;

						case 'a':
							PrincipalServidor_GetHora1(&h1, &m1);
							snprintf(tmp, sizeof(tmp), "%02u:%02u", h1, m1);
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;

						case 'b':
							PrincipalServidor_GetHora2(&h2, &m2);
							snprintf(tmp, sizeof(tmp), "%02u:%02u", h2, m2);
							len = (uint32_t)sprintf(buf, &env[4], tmp);
							break;
						case 'c':
              {
                uint8_t count;
                uint8_t idx_log;

                count = PrincipalServidor_LogGetCount();
                idx_log = MYBUF(pcgi)->idx;

                if (count == 0U) {
                  len = (uint32_t)sprintf(buf,
                    "<tr align=\"center\">"
                    "<td colspan=\"4\">No hay eventos guardados</td>"
                    "</tr>\r\n");

                  MYBUF(pcgi)->idx = 0U;
                }
                else if ((idx_log < count) &&
                         (PrincipalServidor_LogGetByIndex(idx_log, &log) == 0)) {

                  len = (uint32_t)sprintf(buf,
                    "<tr align=\"center\">"
                    "<td>%02u/%02u/20%02u</td>"
                    "<td>%02u:%02u:%02u</td>"
                    "<td>%s</td>"
                    "<td>%s</td>"
                    "</tr>\r\n",
                    (unsigned int)log.day,
                    (unsigned int)log.month,
                    (unsigned int)log.year,
                    (unsigned int)log.hour,
                    (unsigned int)log.minute,
                    (unsigned int)log.second,
                    PrincipalServidor_LogEventoTexto(log.evento),
                    PrincipalServidor_LogModoTexto(log.modo));

                  MYBUF(pcgi)->idx = idx_log + 1U;

                  if (MYBUF(pcgi)->idx < count) {
                    len |= (1U << 31);
                  }
                  else {
                    MYBUF(pcgi)->idx = 0U;
                  }
                }
                else {
                  MYBUF(pcgi)->idx = 0U;
                }

               break;
              }

            case 'd':
              snprintf(tmp, sizeof(tmp), "%u/%u eventos guardados",
                       (unsigned int)PrincipalServidor_LogGetCount(),
                       (unsigned int)SERVIDOR_LOG_MAX);

              len = (uint32_t)sprintf(buf, &env[4], tmp);
              break;
					}
					break;
				}
    
  }
  return (len);
}

#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic pop
#endif

