#ifndef __RTC_H
#define __RTC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

void RTC_Module_Init(void);

const char *RTC_GetTimeString(void);
const char *RTC_GetDateString(void);

HAL_StatusTypeDef RTC_SetDateTime(uint8_t year, uint8_t month, uint8_t date,
                                  uint8_t hours, uint8_t minutes, uint8_t seconds,
                                  uint8_t weekday);

HAL_StatusTypeDef RTC_GetDateTime(RTC_DateTypeDef *sDate, RTC_TimeTypeDef *sTime);

void RTC_SetToDefault2000(void);

#endif
