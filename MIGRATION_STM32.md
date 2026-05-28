# STM32F411 (BlackPill) Porting History

This document chronicles the migration of the Lynxmotion CH3R PS2 project from the 8-bit Arduino Pro Mini to the 32-bit STM32F411.

## 🏁 Final Benchmarks (STM32F411 @ 100MHz)

| Metric | Original (Arduino 16MHz) | STM32 (Integer) | STM32 (Float FPU) |
| :--- | :--- | :--- | :--- |
| **Logic Frame Budget** | 20,000 µs | 20,000 µs | 20,000 µs |
| **Kinematics & Gait** | ~12,000 µs | ~3,331 µs | **~814 µs** |
| **Serial Overhead** | ~14,000 µs (Blocking) | ~14,000 µs | **~3 µs** (Hardware) |
| **Total CPU Load** | ~100% (Saturated) | ~87% | **~4%** |

## 🏗️ Evolution of the Port

### Step 1: OO Refactoring (Arduino Context)
Before touching the hardware, the code was refactored into a modular Object-Oriented structure. This allowed for swapping hardware drivers (PS2, SSC-32) without altering the kinematic math.

### Step 2: 32-bit Modernization
The code was transitioned to **VSCode + PlatformIO**. Core changes included:
- Swapping legacy `byte` and `boolean` types for explicit `uint8_t` and `bool` to resolve C++17 ARM toolchain conflicts (e.g., `std::byte`).
- Removing 8-bit `PROGMEM` hacks. In the unified memory space of the STM32, direct array access is faster and cleaner.
- Adjusting `SoundNoTimer` to use portable `digitalWrite` logic.

### Step 3: Hardware Communication Upgrades
- **SSC-32 (UART):** Moved from blocking `SoftwareSerial` to Hardware **USART2** (`PA2/PA3`). This eliminated a 14ms bottleneck.
- **PS2 Controller (SPI):** Moved from blocking bit-bang logic to Hardware **SPI1** (`PA4-PA7`).
- **Timing Hurdle:** The STM32 SPI hardware was so fast (100MHz) that it "ran over" the wireless receiver. We implemented a strict **20µs inter-byte delay** to synchronize with the receiver's internal processing speed.

### Step 4: The Float FPU Migration
The core math was rewritten from complex 16-bit scaled integers (`c1DEC`, `c2DEC`) to native `float` using standard `<math.h>` functions (`sinf`, `cosf`, `atan2f`). 
- **Result:** Smoother physical motion and a drastically more readable codebase.
- **Precision:** Removed 1-degree resolution errors from legacy lookup tables.

## ⚠️ Lessons Learned

1.  **5V Safety:** The BotBoard II's 1kΩ pull-up resistors to 5V on `PA7` had to be removed to protect the STM32's high-speed SPI drivers.
2.  **Library Sensitivity:** Most standard PS2 libraries use hardcoded AVR registers. We successfully bypassed this by implementing a simplified, local hardware-accelerated driver in the `lib/` folder.
3.  **USB CDC Persistence:** Native USB Serial on STM32 requires `-D PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` and `-D USBCON` flags to be visible to the PC.
4.  **SSC-32 Startup Quirk:** When servos are "freed" (PWM off), the SSC-32 loses track of their position. The very first move command sent after power-on will always execute at maximum speed, ignoring move-time parameters. **Solution:** We implemented a 2-step startup sequence that engages the motors at height 0 (sitting) first to establish a reference point, followed by a smooth 600ms stand-up move.

---
*Migration verified against 20,250 IK master points and 100 gait cycles.*
