#ifndef __BAJOCONSUMO_H
#define __BAJOCONSUMO_H

#include <stdint.h>

int LowPower_Init(void);

void LowPower_BusyEnter(void);
void LowPower_BusyExit(void);

uint8_t LowPower_IsSleeping(void);

#endif