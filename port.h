#ifndef PORT_H_
#define PORT_H_

void PortF_Init(void);
void PortB_Init(void);
void PortE_Init(void);

/* Onboard switches and LEDs: Port F. */
#define SW1          (1U << 4)  /* PF4: onboard switch 1. */
#define SW2          (1U << 0)  /* PF0: onboard switch 2. */
#define RED_LED      (1U << 1)  /* PF1. */
#define BLUE_LED     (1U << 2)  /* PF2. */
#define GREEN_LED    (1U << 3)  /* PF3. */

/* External contestant switches are on different ports. */
#define SWITCH1      (1U << 2)  /* PE2: contestant 1. */
#define SWITCH2      (1U << 3)  /* PB3: contestant 2. */

/* External LEDs: Port B. */
#define RED_LED1     (1U << 4)  /* PB4. */
#define RED_LED2     (1U << 5)  /* PB5. */
#define YELLOW_LED1  (1U << 6)  /* PB6. */
#define YELLOW_LED2  (1U << 7)  /* PB7. */

#define LEDS             (RED_LED1 | RED_LED2 | YELLOW_LED1 | YELLOW_LED2)
#define PORTB_USED_PINS  (SWITCH2 | LEDS)

#endif /* PORT_H_ */
