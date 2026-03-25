#include "rtc.h"
#include <stdio.h>

#define RTC_ASYNCH_PREDIV   0x7F
#define RTC_SYNCH_PREDIV    0x00FF
#define RTC_BKP_MAGIC       0x32F2

static RTC_HandleTypeDef RtcHandle;
static char rtc_time_str[16];
static char rtc_date_str[16];

static void RTC_CalendarConfig(void);

void RTC_Module_Init(void)
{
  RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_NONE;
  RCC_OscInitStruct.LSEState       = RCC_LSE_ON;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    while (1) {}
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    while (1) {}
  }

  __HAL_RCC_RTC_ENABLE();

  RtcHandle.Instance = RTC;
  RtcHandle.Init.HourFormat     = RTC_HOURFORMAT_24;
  RtcHandle.Init.AsynchPrediv   = RTC_ASYNCH_PREDIV;
  RtcHandle.Init.SynchPrediv    = RTC_SYNCH_PREDIV;
  RtcHandle.Init.OutPut         = RTC_OUTPUT_DISABLE;
  RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  RtcHandle.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;

  if (HAL_RTC_Init(&RtcHandle) != HAL_OK) {
    while (1) {}
  }

  if (HAL_RTCEx_BKUPRead(&RtcHandle, RTC_BKP_DR1) != RTC_BKP_MAGIC) {
    RTC_CalendarConfig();
  }
}

static void RTC_CalendarConfig(void)
{
  (void)RTC_SetDateTime(26, 3, 9, 10, 0, 0, RTC_WEEKDAY_MONDAY);
}

HAL_StatusTypeDef RTC_SetDateTime(uint8_t year, uint8_t month, uint8_t date,
                                  uint8_t hours, uint8_t minutes, uint8_t seconds,
                                  uint8_t weekday)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  sTime.Hours          = hours;
  sTime.Minutes        = minutes;
  sTime.Seconds        = seconds;
  sTime.TimeFormat     = RTC_HOURFORMAT12_AM;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sDate.Year    = year;
  sDate.Month   = month;
  sDate.Date    = date;
  sDate.WeekDay = weekday;

  if (HAL_RTC_SetTime(&RtcHandle, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_ERROR;
  }

  if (HAL_RTC_SetDate(&RtcHandle, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_ERROR;
  }

  HAL_RTCEx_BKUPWrite(&RtcHandle, RTC_BKP_DR1, RTC_BKP_MAGIC);
  return HAL_OK;
}

HAL_StatusTypeDef RTC_GetDateTime(RTC_DateTypeDef *sDate, RTC_TimeTypeDef *sTime)
{
  if ((sDate == NULL) || (sTime == NULL)) {
    return HAL_ERROR;
  }

  if (HAL_RTC_GetTime(&RtcHandle, sTime, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_ERROR;
  }

  if (HAL_RTC_GetDate(&RtcHandle, sDate, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

void RTC_SetToDefault2000(void)
{
  (void)RTC_SetDateTime(0, 1, 1, 0, 0, 0, RTC_WEEKDAY_SATURDAY);
}

const char *RTC_GetTimeString(void)
{
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

  if (RTC_GetDateTime(&sDate, &sTime) != HAL_OK) {
    snprintf(rtc_time_str, sizeof(rtc_time_str), "--:--:--");
    return rtc_time_str;
  }

  snprintf(rtc_time_str, sizeof(rtc_time_str), "%02d:%02d:%02d",
           sTime.Hours, sTime.Minutes, sTime.Seconds);

  return rtc_time_str;
}

const char *RTC_GetDateString(void)
{
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

  if (RTC_GetDateTime(&sDate, &sTime) != HAL_OK) {
    snprintf(rtc_date_str, sizeof(rtc_date_str), "--/--/----");
    return rtc_date_str;
  }

  snprintf(rtc_date_str, sizeof(rtc_date_str), "%02d-%02d-%04d",
           sDate.Date, sDate.Month, 2000 + sDate.Year);

  return rtc_date_str;
}
