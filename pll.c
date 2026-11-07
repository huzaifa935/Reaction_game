/*
 * pll.c
 *
 *  Created on: Oct 10, 2026
 *      Author: 16022
 */





#include "tm4c123gh6pm.h"
#include "pll.h"
#include "stdint.h"

/*
 * In the RCC register the sysdiv feild speifies which divisor is used
 * to generate the system clock form either the PLL ouput or the oscillator
 * (Selecting form the System clock depends on the by pass = 0 for PLL and 1 for
 * another clock source) and When using the PLL, the VCO frequency of 400 MHz is predivided by 2 before the
   divisor is applied.  Note we use RCC2 for additional feature that are not present
   in the RCC . The featrue are 1. SYSDIV2  2. PWRDN2  3.BYPASS2  4.OSCRC2
 *
 */
void pll_configure(void)
 {
    /* 16 MHz crystal, 400 MHz PLL / 5 = 80 MHz */
     SYSCTL_RCC_R |= (1U << 11);             /* Bypass PLL during setup */
     SYSCTL_RCC_R &= ~(1U << 22);            /* Bypass system divider */
     SYSCTL_RCC_R = (SYSCTL_RCC_R & ~0x7C0U) | 0x540U;
     SYSCTL_RCC_R &= ~(1U << 0);             /* Enable main oscillator */

     SYSCTL_RCC2_R |= (1U << 31) | (1U << 11); /* USERCC2, BYPASS2 */
     SYSCTL_RCC2_R &= ~0x70U;                /* Select main oscillator */
     SYSCTL_RCC2_R &= ~(1U << 13);           /* Power up PLL */
     SYSCTL_RCC2_R |= (1U << 30);            /* DIV400 */
     SYSCTL_RCC2_R = (SYSCTL_RCC2_R & ~0x1FC00000U) |
                     (2U << 23);             /* Divide by 5 */
     SYSCTL_RCC_R |= (1U << 22);             /* Enable divider */

     while ((SYSCTL_RIS_R & (1U << 6)) == 0U)
     {
     }

     SYSCTL_RCC2_R &= ~(1U << 11);           /* Switch to PLL */
 }
