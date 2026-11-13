#include <stdint.h>              /* uint8_t, uint32_t, int32_t */
#include <stdlib.h>              /* rand() and srand() */

#include "tm4c123gh6pm.h"        /* TM4C123 hardware register names */
#include "port.h"                /* Switch/LED pin masks and PortX_Init() */
#include "pll.h"                 /* pll_configure() */
#include "systick.h"             /* SysTick setup functions */

/*
 * These are the possible phases and outcomes of one game round.
 *
 * IDLE:    Waiting for SW1; onboard blue LED blinks.
 * READYON: Onboard red LED; buzzing is a false start.
 * GO:      Onboard green LED; contestants may buzz.
 * LOCKOUT: A winner was selected, or GO timed out.
 * TIE:     Both saved buzz timestamps were close enough.
 * FALSE:   One or both contestants buzzed during READYON.
 */
typedef enum {
    STATE_IDLE,
    STATE_READYON,
    STATE_GO,
    STATE_LOCKOUT,
    STATE_TIE,
    STATE_FALSE
} reaction_state_t;

/*
 * The current game state.
 *
 * volatile is appropriate because interrupt handlers read state while
 * the main loop changes it.
 */
volatile reaction_state_t state = STATE_IDLE;


extern volatile uint32_t count_ms;


volatile uint8_t sw1_press = 0U, sw2_press = 0U;
volatile uint8_t f1_press = 0U, f2_press = 0U;


uint8_t sw1_down = 0U, sw2_down = 0U;
uint8_t f1_down = 0U, f2_down = 0U;

uint8_t sw1_up = 0U, sw2_up = 0U;
uint8_t f1_up = 0U, f2_up = 0U;


volatile uint32_t f1_count = 0U, f2_count = 0U;


volatile uint8_t f1_stage = 0U, f2_stage = 0U;

/*
 * Confirmed outcomes for the current round.
 *
 * f1_error/f2_error: contestant pressed during READYON.
 * f1_hit/f2_hit:     contestant pressed during GO.
 * winner:            0 = none, 1 = contestant 1, 2 = contestant 2.
 */
uint8_t f1_error = 0U, f2_error = 0U;
uint8_t f1_hit = 0U, f2_hit = 0U;
uint8_t winner = 0U;

/*
 * Saved times used for nonblocking elapsed-time comparisons.
 *
 * blue_blink: last time the IDLE blue LED toggled.
 * idle:       last time ALL four buttons were sampled for debounce.
 * readyon:    time READYON began.
 * go_time:    time GO began.
 * ready_delay: randomly selected READYON duration, in milliseconds.
 */
uint32_t blue_blink = 0U;
uint32_t idle = 0U;
uint32_t readyon = 0U;
uint32_t go_time = 0U;
uint32_t ready_delay = 0U;

/*
 * Check one button's physical level during a scheduled 10 ms sample.
 *
 * data:          GPIO DATA register; tells us HIGH or LOW RIGHT NOW.
 * icr:           GPIO interrupt-clear register.
 * imr:           GPIO interrupt-mask register.
 * position:      bit mask for this particular button.
 * button_press:  its 0/1/2 debounce phase.
 * down/up:       its consecutive LOW/HIGH sample counters.
 *
 * Returns 1 exactly once when a press is confirmed.
 * Returns 0 when there is no new confirmed press.
 *
 * Important: A saved interrupt edge does not prove the switch stayed
 * pressed. That is why this function reads GPIO DATA, not GPIO RIS.
 */
uint8_t deboucing(volatile uint32_t *data,
                  volatile uint32_t *icr,
                  volatile uint32_t *imr,
                  uint32_t position,
                  volatile uint8_t *button_press,
                  uint8_t *down,
                  uint8_t *up)
{
    /*
     * Phase 0: No falling-edge interrupt has requested a check.
     * There is no work to do for this button on this sample.
     */
    if (*button_press == 0U)
        return 0U;

    if (*button_press == 1U)
    {
        /*
         * Phase 1: A falling edge was detected.
         * With an internal pull-up, LOW means the button is pressed.
         */
        if ((*data & position) == 0U)
        {
            *up = 0U; /* A LOW reading breaks any HIGH sample streak. */

            /*
             * Count consecutive LOW readings, one per 10 ms call.
             * The fourth confirms a stable press.
             */
            if (++(*down) == 4U)
            {
                *up = 0U;           // Start release counting from zero
                *down = 0U;
                *button_press = 2U; /* Now held; do not report again. */
                return 1U;         /* One new confirmed-press event. */
            }
        }
        else
        {
            /*
             * The input is HIGH again before a press was confirmed.
             * Clear the incomplete LOW streak.
             */
            *down = 0U;

            /*
             * Four consecutive HIGH samples mean that the suspected
             * press ended. It might have been contact bounce.
             */
            if (++(*up) == 4U)
            {
                *up = 0U;
                *button_press = 0U;

                /* Remove any saved edge, then allow another interrupt. */
                *icr = position;
                *imr |= position;
            }
        }
    }
    else
    {
        /*
         * Phase 2: This press was already reported.
         * We are waiting for the button to stay released.
         */
        if ((*data & position) != 0U)
        {
            /* HIGH means released because the pull-up is active. */
            if (++(*up) == 4U)
            {
                *up = 0U;
                *button_press = 0U;

                /*
                 * Only after a stable release do we re-enable the
                 * falling-edge interrupt for the next press.
                 */
                *icr = position;
                *imr |= position;
            }
        }
        else
        {
            /* It went LOW again: the release was not stable yet. */
            *up = 0U;
        }
    }

    return 0U;
}

/*
 * PORT F INTERRUPT: onboard SW1 and SW2.
 *
 * This handler records WHICH onboard switch produced an interrupt.
 * It does not debounce and does not directly change state.
 */
void gpio_portf_int(void)
{
    /*
     * MIS = masked interrupt status.
     * Keep only the PF4 (SW1) and PF0 (SW2) bits.
     */
    uint32_t status = GPIO_PORTF_MIS_R & (SW1 | SW2);

    /*
     * Mask whichever switch fired. This prevents repeated interrupts
     * from contact bounce while the main loop is debouncing it.
     */
    GPIO_PORTF_IM_R &= ~status;

    /*
     * ICR is write-one-to-clear. Acknowledge the detected edge.
     * deboucing() will later re-enable this switch after release.
     */
    GPIO_PORTF_ICR_R = status;

    if ((status & SW1) != 0U)
        sw1_press = 1U; /* Start sampling PF4. */

    if ((status & SW2) != 0U)
        sw2_press = 1U; /* Start sampling PF0. */
}

/*
 * PORT E INTERRUPT: contestant 1 on PE2.
 */
void gpio_porte_int(void)
{
    uint32_t status = GPIO_PORTE_MIS_R & SWITCH1;

    /* Prevent PE2 from interrupting again while being debounced. */
    GPIO_PORTE_IM_R &= ~status;

    /* Acknowledge this falling edge. */
    GPIO_PORTE_ICR_R = status;

    if (status != 0U)
    {
        /*
         * Capture the first-edge time NOW, not after debounce.
         * Otherwise the measured reaction time would include
         * approximately 40 ms of debounce time.
         */
        f1_count = count_ms;

        /*
         * Capture the phase in which the edge happened.
         * Debounce may finish after the phase has changed.
         */
        f1_stage = (state == STATE_READYON) ? 1U :
                   ((state == STATE_GO) ? 2U : 0U);

        /* Ask the main loop to start checking PE2. */
        f1_press = 1U;
    }
}

/*
 * PORT B INTERRUPT: contestant 2 on PB3.
 *
 * It performs the same job as the Port E handler, but saves the result
 * in contestant 2's separate variables.
 */
void gpio_portb_int(void)
{
    uint32_t status = GPIO_PORTB_MIS_R & SWITCH2;

    GPIO_PORTB_IM_R &= ~status;
    GPIO_PORTB_ICR_R = status;

    if (status != 0U)
    {
        f2_count = count_ms;

        f2_stage = (state == STATE_READYON) ? 1U :
                   ((state == STATE_GO) ? 2U : 0U);

        f2_press = 1U;
    }
}

/*
 * Execute one pass of the nonblocking game state machine.
 *
 * main() calls this repeatedly. It never waits for a button or timer.
 * Timed actions happen only when count_ms shows enough time has passed.
 */
void state_machine(void)
{
    /* Take one snapshot of the millisecond counter for this pass. */
    uint32_t now = count_ms;

    /*
     * These four variables exist only for this one call.
     * deboucing() sets one to 1 if that switch has just produced
     * a confirmed new press.
     */
    uint8_t sw1_event = 0U, sw2_event = 0U;
    uint8_t f1_event = 0U, f2_event = 0U;

    /*
     * Sample all four buttons at most once every 10 ms.
     * The unsigned subtraction also works across uint32_t wraparound
     * for these short elapsed intervals.
     */
    if ((uint32_t)(now - idle) >= 10U)
    {
        idle = now;

        sw1_event = deboucing(&GPIO_PORTF_DATA_R,
                              &GPIO_PORTF_ICR_R,
                              &GPIO_PORTF_IM_R,
                              SW1,
                              &sw1_press,
                              &sw1_down,
                              &sw1_up);

        sw2_event = deboucing(&GPIO_PORTF_DATA_R,
                              &GPIO_PORTF_ICR_R,
                              &GPIO_PORTF_IM_R,
                              SW2,
                              &sw2_press,
                              &sw2_down,
                              &sw2_up);

        f1_event = deboucing(&GPIO_PORTE_DATA_R,
                             &GPIO_PORTE_ICR_R,
                             &GPIO_PORTE_IM_R,
                             SWITCH1,
                             &f1_press,
                             &f1_down,
                             &f1_up);

        f2_event = deboucing(&GPIO_PORTB_DATA_R,
                             &GPIO_PORTB_ICR_R,
                             &GPIO_PORTB_IM_R,
                             SWITCH2,
                             &f2_press,
                             &f2_down,
                             &f2_up);
    }

    /*
     * SW2 is checked BEFORE switch(state), so its reset action is
     * available from every game state.
     */
    if (sw2_event != 0U)
    {
        state = STATE_IDLE;

        /* Forget the previous round's outcome and captured stages. */
        f1_error = f2_error = f1_hit = f2_hit = winner = 0U;
        f1_stage = f2_stage = 0U;

        /* Turn off the external LEDs and the onboard RGB LED. */
        GPIO_PORTB_DATA_R &= ~LEDS;
        GPIO_PORTF_DATA_R &= ~(RED_LED | BLUE_LED | GREEN_LED);

        /* Start a fresh one-second blue-blink interval. */
        blue_blink = now;
        return;
    }

    switch (state)
    {
        case STATE_IDLE:
            /*
             * Every 1000 ms, invert only PF2.
             * ^= BLUE_LED changes the blue bit without intentionally
             * changing the other Port F bits.
             */
            if ((uint32_t)(now - blue_blink) >= 1000U)
            {
                blue_blink = now;
                GPIO_PORTF_DATA_R ^= BLUE_LED;
            }

            /*
             * In THIS version of your code, SW1 is handled only here.
             * Therefore SW1 cannot start a new round from another state.
             */
            if (sw1_event != 0U)
            {
                GPIO_PORTF_DATA_R &= ~BLUE_LED;
                GPIO_PORTB_DATA_R &= ~LEDS;

                /* Clear the previous round's outcome. */
                f1_error = f2_error = f1_hit = f2_hit = winner = 0U;

                /*
                 * Use the human SW1 timing as a seed.
                 * rand() % 3001 gives 0..3000.
                 * Therefore this expression gives 2000..5000 ms.
                 */
                srand(now);
                ready_delay = 2000U + (uint32_t)(rand() % 3001U);

                readyon = now;         /* READYON starting time. */
                state = STATE_READYON;
            }
            break;

        case STATE_READYON:
            /*
             * Clear blue/green, then set the onboard red bit.
             * This is the "wait; do not buzz" phase.
             */
            GPIO_PORTF_DATA_R =
                (GPIO_PORTF_DATA_R & ~(BLUE_LED | GREEN_LED)) | RED_LED;

            /*
             * A newly confirmed contestant press is a false start only
             * if its FIRST edge was recorded during READYON.
             */
            if ((f1_event != 0U) && (f1_stage == 1U))
                f1_error = 1U;

            if ((f2_event != 0U) && (f2_stage == 1U))
                f2_error = 1U;

            if ((f1_error != 0U) || (f2_error != 0U))
            {
                /*
                 * If another contestant's press is still in phase 1,
                 * wait for its debounce result before deciding.
                 */
                if ((f1_press != 1U) && (f2_press != 1U))
                {
                    if ((f1_error != 0U) &&
                        (f2_error != 0U) &&
                        (f1_count == f2_count))
                    {
                        /* Both false starts have the same saved ms tick. */
                        winner = 0U;
                    }
                    else if ((f1_error != 0U) &&
                             (f2_error != 0U))
                    {
                        /*
                         * Both made false starts at different saved times.
                         * This code chooses the opponent of the earlier
                         * offender as winner. It can therefore show
                         * both yellow LEDs AND one red LED.
                         */
                        winner =
                            ((int32_t)(f1_count - f2_count) < 0)
                            ? 2U : 1U;
                    }
                    else
                    {
                        /* One offender: the other contestant wins. */
                        winner = (f1_error != 0U) ? 2U : 1U;
                    }

                    state = STATE_FALSE;
                }
            }
            else if ((f1_press != 1U) &&
                     (f2_press != 1U) &&
                     ((uint32_t)(now - readyon) >= ready_delay))
            {
                /*
                 * No confirmed false start and no press still being
                 * checked. The random READYON wait has expired.
                 */
                go_time = now; /* Start the 3-second GO window. */

                GPIO_PORTF_DATA_R =
                    (GPIO_PORTF_DATA_R & ~RED_LED) | GREEN_LED;

                state = STATE_GO;
            }
            break;

        case STATE_GO:
            /* Show the onboard green LED; clear blue and red. */
            GPIO_PORTF_DATA_R =
                (GPIO_PORTF_DATA_R & ~(BLUE_LED | RED_LED)) | GREEN_LED;

            /*
             * A confirmed press counts as a GO hit when:
             *  1. Its first edge occurred during GO; and
             *  2. Its saved edge time is inside the 3000 ms GO window.
             */
            if ((f1_event != 0U) &&
                (f1_stage == 2U) &&
                ((uint32_t)(f1_count - go_time) < 3000U))
            {
                f1_hit = 1U;
            }

            if ((f2_event != 0U) &&
                (f2_stage == 2U) &&
                ((uint32_t)(f2_count - go_time) < 3000U))
            {
                f2_hit = 1U;
            }

            if ((f1_hit != 0U) || (f2_hit != 0U))
            {
                /*
                 * Do not decide while either contestant's press
                 * still needs debounce confirmation.
                 */
                if ((f1_press != 1U) && (f2_press != 1U))
                {
                    if ((f1_hit != 0U) &&
                        (f2_hit != 0U) &&
                        (
                            ((f1_count >= f2_count) &&
                             (f1_count - f2_count <= 1U))
                            ||
                            ((f2_count > f1_count) &&
                             (f2_count - f1_count <= 1U))
                        ))
                    {
                        /*
                         * Both hits were confirmed and their saved
                         * millisecond counts differ by 0 or 1.
                         *
                         * Comparing both subtraction directions prevents
                         * a negative time difference from accidentally
                         * satisfying an unsigned <= 1 test.
                         */
                        state = STATE_TIE;
                    }
                    else
                    {
                        if ((f1_hit != 0U) && (f2_hit != 0U))
                        {
                            /* Both hit, but outside your tie window. */
                            winner =
                                ((int32_t)(f1_count - f2_count) < 0)
                                ? 1U : 2U;
                        }
                        else
                        {
                            /* Only one contestant produced a valid hit. */
                            winner = (f1_hit != 0U) ? 1U : 2U;
                        }

                        state = STATE_LOCKOUT;
                    }
                }
            }
            else if (((uint32_t)(now - go_time) >= 3000U) &&
                     (f1_press != 1U) &&
                     (f2_press != 1U))
            {
                /*
                 * Three seconds passed with no valid hit and no press
                 * still being checked. LOCKOUT will display timeout.
                 */
                winner = 0U;
                state = STATE_LOCKOUT;
            }
            break;

        case STATE_LOCKOUT:
            /*
             * Clear onboard RGB. On a timeout (winner == 0),
             * set onboard BLUE steadily.
             */
            GPIO_PORTF_DATA_R =
                (GPIO_PORTF_DATA_R &
                 ~(RED_LED | BLUE_LED | GREEN_LED)) |
                ((winner == 0U) ? BLUE_LED : 0U);

            /*
             * Clear all four external LED bits first.
             * Winner 1 -> Win 1 red and contestant 2 yellow.
             * Winner 2 -> Win 2 red and contestant 1 yellow.
             * No winner -> no external LEDs.
             */
            GPIO_PORTB_DATA_R =
                (GPIO_PORTB_DATA_R & ~LEDS) |
                ((winner == 1U)
                 ? (RED_LED1 | YELLOW_LED2) : 0U) |
                ((winner == 2U)
                 ? (RED_LED2 | YELLOW_LED1) : 0U);
            break;

        case STATE_TIE:
            /* Turn off onboard RGB; show both external red Win LEDs. */
            GPIO_PORTF_DATA_R &=
                ~(RED_LED | BLUE_LED | GREEN_LED);

            GPIO_PORTB_DATA_R =
                (GPIO_PORTB_DATA_R & ~LEDS) |
                RED_LED1 | RED_LED2;
            break;

        case STATE_FALSE:
            /* Your FALSE state turns off all onboard RGB colors. */
            GPIO_PORTF_DATA_R &=
                ~(BLUE_LED | GREEN_LED | RED_LED);

            /*
             * Yellow LED 1 = contestant 1 made a false start.
             * Yellow LED 2 = contestant 2 made a false start.
             * The red LED follows the winner selected in READYON.
             *
             * This is equivalent to your expression but removes its
             * unnecessary assignment nested inside another assignment.
             */
            GPIO_PORTB_DATA_R =
                (GPIO_PORTB_DATA_R & ~LEDS) |
                (f1_error ? YELLOW_LED1 : 0U) |
                (f2_error ? YELLOW_LED2 : 0U) |
                ((winner == 1U) ? RED_LED1 : 0U) |
                ((winner == 2U) ? RED_LED2 : 0U);
            break;

        default:
            /* Recover to a known state if state has an invalid value. */
            state = STATE_IDLE;
            break;
    }
}

int main(void)
{
    pll_configure();

    /* Configure contestant 1, contestant 2/LEDs, and onboard controls. */
    PortE_Init();
    PortB_Init();
    PortF_Init();

    SysTick_SetPeriodAndStart(80000000U, 1U);

    /* Permit the periodic SysTick interrupt to run. */
    SysTick_EnableInterrupt(1U);

    while (1)
        state_machine();
}
