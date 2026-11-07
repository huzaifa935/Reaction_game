/*
 * systick.h
 *
 *  Created on: Oct 10, 2026
 *      Author: 16022
 */

#ifndef SYSTICK_H_
#define SYSTICK_H_

#include "tm4c123gh6pm.h"
#include <stdint.h>

#define SYSTICK_ENABLE  (1U << 0)
#define SYSTICK_INTEN   (1U << 1)
#define SYSTICK_CLK_SRC (1U << 2)
void SysTick_EnableInterrupt(uint8_t enable);
uint8_t SysTick_SetPeriodAndStart(uint32_t clock_hz, uint32_t period_ms);



// Uses the elapsed_ms variable from the previous example.


#endif /* SYSTICK_H_ */
