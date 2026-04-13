# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
cmake --preset debug

# Build
cmake --build build/debug

# Release build
cmake --preset release && cmake --build build/release
```

Build artifacts: `build/debug/stm32f407-reg-dev.elf/.bin/.hex`

Linker warnings about `_close`, `_lseek`, `_read` not implemented are expected noise for bare-metal (newlib-nano stubs) — not errors.

## Adding a New Peripheral Module

Each module lives in `peripherals/<name>/` with `<name>.c`, `<name>.h`, and `<name>.md`. When adding one:

1. Create `peripherals/<name>/<name>.c` and `<name>.h`
2. Add `peripherals/<name>/<name>.c` to `add_executable()` in `CMakeLists.txt`
3. Add `peripherals/<name>` to `target_include_directories()` in `CMakeLists.txt`
4. Add a section to `README.md` under `## 外设驱动`
5. Write a `<name>.md` documenting registers, pin mapping, clock source, and function API

## Architecture

**No HAL/LL libraries.** Every peripheral is configured by writing directly to hardware registers via the CMSIS structs in `core/inc/` (e.g., `GPIOF->MODER`, `TIM14->CCR1`).

**Clock topology** (set in `sys.c`):
- HSE 8MHz → PLL → SYSCLK 168MHz
- AHB 168MHz, APB1 42MHz (timers ×2 = 84MHz), APB2 84MHz (timers ×2 = 168MHz — not used here)
- `delay_init()` must follow `sys_clock_init()` (reads `SystemCoreClock`)

**Timer module consolidation** — all timer drivers share `peripherals/timer/timer.c/.h`:
| Timer | Function | Key pin |
|-------|----------|---------|
| TIM3  | Periodic update interrupt | — |
| TIM5  | Input capture (pulse width) | PA0 AF2 |
| TIM2  | Capacitive key (charge-discharge) | PA5 AF1 |
| TIM14 | PWM output (LED0 brightness) | PF9 AF9 |

**Initialization order constraints** in `main.c`:
```c
sys_clock_init();   // first — sets 168MHz
delay_init();       // needs SystemCoreClock
key_init();         // configures PA0 pull-down before TIM5 takes over the pin
tim5ic_init();      // switches PA0 to AF2 (TIM5_CH1)
tim2cap_init();     // auto-calibrates baseline — call with no touch on PA5
```

**LED polarity**: LED0 (PF9) and LED1 (PF10) are active-low. TIM14 CCR1 = `(100 - duty) × 10`.

**IWDG**: Once started it cannot be stopped (survives reset, only stopped by power-off). To freeze it during debug: `DBGMCU->APB1FZ |= (1U << 12)` before `iwdg_init()`.

**UART RX**: Ring buffer (64 bytes) filled by USART1 ISR; `printf` is redirected to USART1 TX via `_write` in `uart.c`.

## Documentation Convention

Every module's `.md` file documents: pin/clock table, relevant register bit-fields, configuration procedure, and public API. Keep `.md` files in sync with source — discrepancies are bugs.
