#ifndef __SNTP_H
#define __SNTP_H

#include "cmsis_os2.h"

void SNTP_Init(void);
void SNTP_ForceSyncNow(void);

extern osThreadId_t tid_sntp;

#endif
