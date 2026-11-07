# Two-Player Reaction Timer | TM4C123

A two-player reaction game written in bare-metal C for the TI Tiva C Series TM4C123GH6PM LaunchPad. After a random waiting period, a green LED signals both players to press their buttons. The firmware compares button timestamps, indicates the winner, and handles false starts and ties.

The project demonstrates GPIO interrupts, software debouncing, SysTick timing, PLL configuration, and a finite state machine.

## Features

- Two external contestant buttons with separate winner and false-start LEDs.
- Random ready delay between 2 and 5 seconds.
- GPIO interrupt timestamps captured before software debounce confirmation.
- A 1 ms SysTick time base with an 80 MHz system clock.
- False-start detection when a contestant presses during the ready period.
- Tie detection when both confirmed presses have the same saved tick count.
- A 3-second response timeout and a reset control.
- Game timing and debouncing handled without blocking delay loops.

## Hardware and tools

- TM4C123GH6PM LaunchPad (EK-TM4C123GXL).
- Two external pushbuttons.
- Four external LEDs: two winner LEDs and two false-start LEDs.
- Suitable LED series resistors, a breadboard, and jumper wires.
- Code Composer Studio with the device header, startup file, and linker configuration for the target.

## Pin connections

| Function | Pin | Connection or purpose |
|---|---|---|
| Start | PF0 | Onboard button; starts a round from IDLE |
| Reset | PF4 | Onboard button; returns to IDLE |
| Onboard red LED | PF1 | Ready indication |
| Onboard blue LED | PF2 | Idle indication |
| Onboard green LED | PF3 | GO indication |
| Player 1 button | PB2 | Pushbutton connected to ground |
| Player 2 button | PB3 | Pushbutton connected to ground |
| Player 1 winner LED | PB4 | Active-high output |
| Player 2 winner LED | PB5 | Active-high output |
| Player 1 false-start LED | PB6 | Active-high output |
| Player 2 false-start LED | PB7 | Active-high output |

The contestant inputs use internal pull-ups, so a button press produces a falling edge. Connect each external LED output through a suitable series resistor and LED to ground.

**Switch naming:** In this source version, `port.h` defines `SW1` as PF0 and `SW2` as PF4. Those macro names are reversed relative to the LaunchPad's printed button labels: the physical SW2 is PF0, and the physical SW1 is PF4. Use the pin assignments above when operating this version.

## How to play

1. Power the board and run the firmware. The blue LED blinks in IDLE.
2. Press the start button on PF0. The red LED turns on during the random ready delay.
3. Wait for the green LED before pressing a contestant button.
4. After GO, the earlier valid contestant timestamp determines the winner. The corresponding external winner LED turns on.
5. If both valid contestant timestamps are equal, the onboard RGB LED turns white to indicate a tie.
6. A contestant press during the ready period ends the round as a false start and lights that player's error LED.
7. Press reset on PF4 to clear a result, then press PF0 to start another round.

If neither contestant produces a valid press before the GO timeout, the firmware returns to IDLE after any pending button checks finish.

## State machine

| State | Behavior |
|---|---|
| `STATE_IDLE` | Blinks blue and waits for start |
| `STATE_READYON` | Shows red and waits for the random delay; detects early presses |
| `STATE_GO` | Shows green and collects valid contestant presses |
| `STATE_LOCKOUT` | Holds the winner indication until reset |
| `STATE_TIE` | Holds a white onboard LED indication until reset |
| `STATE_FALSE` | Holds the false-start indication until reset |

The reset button returns the game to IDLE from any state.

## Timing and button handling

| Setting | Value |
|---|---|
| System clock | 80 MHz, configured by the PLL |
| SysTick period | 1 ms |
| Ready delay | 2000-5000 ms, inclusive |
| GO timeout | 3000 ms |
| Button sampling interval | 5 ms |
| Press confirmation | 3 consecutive low samples |
| Release confirmation | 3 consecutive high samples |
| Initial decision window | 20 ms; pending contestant checks must also finish |

A GPIO handler saves the current tick count and game state, then masks that button's interrupt. The main loop checks the input to confirm a stable press. After a stable release, it clears pending interrupt flags and rearms the button.

Winner selection uses the saved interrupt timestamp rather than the time at which debouncing finishes. The tie rule is **equal saved tick counts**, rather than a sliding 1 ms difference between presses. Resolution is 1 ms; interrupt latency also affects the recorded timestamps, and sub-millisecond ordering is not available in this implementation.

The firmware displays results using LEDs. A numerical reaction-time display is not implemented in this source version.

## Source files

| File | Purpose |
|---|---|
| `main.c` | Game states, contestant timestamps, debouncing, and GPIO handlers |
| `port.c` / `port.h` | GPIO configuration, pin definitions, and GPIO interrupt setup |
| `pll.c` / `pll.h` | System clock configuration |
| `systick.c` / `systick.h` | Millisecond counter and SysTick interrupt handlers |

## Build and run

1. Create a Code Composer Studio project for the TM4C123GH6PM device.
2. Add the source files and matching headers listed above.
3. Include `tm4c123gh6pm.h` and the target's startup and linker files. These support files are required in addition to the game modules.
4. Connect the startup vector entries to the handlers below. Each row lists alternative names supplied by the source; use one name per vector entry.

   | Interrupt | Handler names |
   |---|---|
   | SysTick | `SysTickIntHandler` or `SysTick_Handler` |
   | GPIO Port F | `GPIOPortF_Handler` or `GPIOPortFIntHandler` |
   | GPIO Port B | `GPIOPortB_Handler` or `GPIOPortBIntHandler` |

5. Build the project, connect the LaunchPad through its debug USB port, and load the firmware.
6. Run the program and follow the controls above.

## Suggested hardware checks

- Confirm that each contestant can win a round.
- Check false-start detection for each contestant.
- Check that held buttons and contact bounce do not produce repeated presses.
- Check reset during the ready period, during GO, and after each result.
- Check the return to IDLE when neither contestant responds.
- Exercise the equal-timestamp path to check the tie indication.

## Reference

[TI TM4C123G LaunchPad User's Guide](https://www.ti.com/lit/ug/spmu296/spmu296.pdf)

## Author
Huzaifa Muhmmad 

[Huzaifa Mohammed](https://github.com/huzaifa935)
