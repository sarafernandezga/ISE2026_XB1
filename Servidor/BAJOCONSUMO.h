#ifndef __BAJOCONSUMO_H
#define __BAJOCONSUMO_H

#include <stdint.h>

void LowPower_Init(void);
void LowPower_RequestSleep(void);
uint8_t LowPower_IsSleeping(void);

#endif