/*
 * port.c
 *
 * Created on: Oct 10, 2026
 * Author: 16022
 */
#include "tm4c123gh6pm.h"
#include "port.h"
#include "stdint.h"
void PortF_Init(void)
{
    SYSCTL_RCGCGPIO_R |= (1U << 5);
    while ((SYSCTL_PRGPIO_R & (1U << 5)) == 0U)
    {
        /* Wait for Port F to become ready. */
    }

    GPIO_PORTF_LOCK_R = 0x4C4F434BU;  /* Unlock PF0. */
    GPIO_PORTF_CR_R |= 0x1FU;

    GPIO_PORTF_AMSEL_R &= ~0x1FU;
    GPIO_PORTF_PCTL_R  &= ~0x000FFFFFU;
    GPIO_PORTF_AFSEL_R &= ~0x1FU;
    GPIO_PORTF_DIR_R   &= ~(SW1 | SW2);
    GPIO_PORTF_DIR_R   |= (RED_LED | BLUE_LED | GREEN_LED);
    GPIO_PORTF_PUR_R   |= (SW1 | SW2);
    GPIO_PORTF_DEN_R   |= 0x1FU;

    GPIO_PORTF_DATA_R &= ~(RED_LED | BLUE_LED | GREEN_LED);

    /* Onboard buttons: falling-edge interrupts. */
    GPIO_PORTF_IM_R  &= ~(SW1 | SW2);
    GPIO_PORTF_IS_R  &= ~(SW1 | SW2);
    GPIO_PORTF_IBE_R &= ~(SW1 | SW2);
    GPIO_PORTF_IEV_R &= ~(SW1 | SW2);
    GPIO_PORTF_ICR_R  =  (SW1 | SW2);
    GPIO_PORTF_IM_R  |=  (SW1 | SW2);

    NVIC_EN0_R |= (1U << 30);
}

void PortB_Init(void)
{
    SYSCTL_RCGCGPIO_R |= (1U << 1);
    while ((SYSCTL_PRGPIO_R & (1U << 1)) == 0U)
    {
        /* Wait for Port B to become ready. */
    }

    /* PB3 is the only Port B switch; PB4-PB7 are LEDs. */
    GPIO_PORTB_AMSEL_R &= ~PORTB_USED_PINS;
    GPIO_PORTB_PCTL_R  &= ~0xFFFFF000U;  /* PB3-PB7 are GPIO. */
    GPIO_PORTB_AFSEL_R &= ~PORTB_USED_PINS;
    GPIO_PORTB_DATA_R  &= ~LEDS;
    GPIO_PORTB_DIR_R   &= ~SWITCH2;
    GPIO_PORTB_DIR_R   |= LEDS;
    GPIO_PORTB_PUR_R   |= SWITCH2;
    GPIO_PORTB_DEN_R   |= PORTB_USED_PINS;

    /* PB3: falling-edge interrupt when pressed. */
    GPIO_PORTB_IM_R  &= ~SWITCH2;
    GPIO_PORTB_IS_R  &= ~SWITCH2;
    GPIO_PORTB_IBE_R &= ~SWITCH2;
    GPIO_PORTB_IEV_R &= ~SWITCH2;
    GPIO_PORTB_ICR_R  =  SWITCH2;
    GPIO_PORTB_IM_R  |=  SWITCH2;

    NVIC_EN0_R |= (1U << 1);
}

void PortE_Init(void)
{
    SYSCTL_RCGCGPIO_R |= (1U << 4);
    while ((SYSCTL_PRGPIO_R & (1U << 4)) == 0U)
    {
        /* Wait for Port E to become ready. */
    }

    /* PE2 is the other external switch. */
    GPIO_PORTE_DIR_R   &= ~SWITCH1;
    GPIO_PORTE_AFSEL_R &= ~SWITCH1;
    GPIO_PORTE_AMSEL_R &= ~SWITCH1;
    GPIO_PORTE_PCTL_R  &= ~(0xFU << 8);  /* PE2 is GPIO. */
    GPIO_PORTE_PUR_R   |=  SWITCH1;
    GPIO_PORTE_DEN_R   |=  SWITCH1;

    /* PE2: falling-edge interrupt when pressed. */
    GPIO_PORTE_IM_R  &= ~SWITCH1;
    GPIO_PORTE_IS_R  &= ~SWITCH1;
    GPIO_PORTE_IBE_R &= ~SWITCH1;
    GPIO_PORTE_IEV_R &= ~SWITCH1;
    GPIO_PORTE_ICR_R  =  SWITCH1;
    GPIO_PORTE_IM_R  |=  SWITCH1;

    NVIC_EN0_R |= (1U << 4);
}
