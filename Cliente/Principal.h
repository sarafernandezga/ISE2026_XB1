#ifndef __PRINCIPAL_H
#define __PRINCIPAL_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

#define PRINCIPAL_EVT_RTC       0x1U
#define PRINCIPAL_EVT_COM       0x2U

int Init_ThPrincipal(void);
void Principal_NotifyEvent(uint32_t event_flags);
#endif