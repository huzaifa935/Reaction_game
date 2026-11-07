/*
 * systick.c
 *
 *  Created on: Oct 10, 2026
 *      Author: 16022
 */

#include "systick.h"
#include "tm4c123gh6pm.h"



static uint32_t systick_control = SYSTICK_CLK_SRC;
volatile  uint32_t count_ms =0;
// Set the interval in milliseconds and start SysTick.
// Returns 1 on success, 0 if the interval will not fit the 24-bit timer.
uint8_t SysTick_SetPeriodAndStart(uint32_t clock_hz, uint32_t period_ms)
{
    uint64_t cycles = ((uint64_t)clock_hz * period_ms) / 1000U;

    if (period_ms == 0U || cycles < 2U || cycles > 0x1000000ULL)
        return 0U;

    NVIC_ST_CTRL_R = 0U;                   // Stop while changing the period
    NVIC_ST_RELOAD_R = (uint32_t)cycles - 1U;
    NVIC_ST_CURRENT_R = 0U;                // Clear current count

    // Start counting. Preserve the interrupt choice set by the other function.
    systick_control |= SYSTICK_ENABLE;
    // Using System clock
    NVIC_ST_CTRL_R = systick_control;
    return 1U;
}

// Pass 1 to enable SysTick interrupts; pass 0 to disable them.
void SysTick_EnableInterrupt(uint8_t enable)
{
    if (enable != 0U)
        systick_control |= SYSTICK_INTEN;
    else
        systick_control &= ~SYSTICK_INTEN;

    NVIC_ST_CTRL_R = systick_control;
}

// Read the hardware counter: clock cycles remaining in the current interval.
uint32_t SysTick_ReadCurrent(void)
{
    return NVIC_ST_CURRENT_R;
}

// Use the handler name specified in your project's startup file.
void systick_handler(void)
{
    count_ms++;
}
