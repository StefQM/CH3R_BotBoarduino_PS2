# BotBoarduino CH3R Hexapod - STM32 Modernized Edition

This repository contains the modernized, Object-Oriented C++ firmware for the Lynxmotion CH3R Hexapod. Originally designed for the 8-bit ATmega328P, this codebase has been fully migrated and optimized for the **STM32F411CEU6 (BlackPill)**.

## 🚀 Architectural Highlights

- **Object-Oriented Design:** The logic is encapsulated into modular classes (`Hexapod`, `Leg`, `InputController`, `ServoDriver`), making the code extensible and easy to maintain.
- **Hardware Acceleration:**
    - **Native Hardware SPI:** PS2 controller polling is handled by the STM32 SPI1 peripheral, eliminating blocking software delays.
    - **Hardware USART:** High-speed communication with the SSC-32 servo controller uses USART2, freeing up the CPU during movement updates.
    - **Hardware FPU:** Core kinematics and gait calculations utilize the STM32's Floating Point Unit for extreme precision and smooth motion.
- **Ultra-Low CPU Load:** The entire robot brain executes in **~850µs**, utilizing only **~4%** of the available 100MHz CPU power (per 20ms frame).
- **USB CDC Console:** Live telemetry and the Terminal Monitor are accessible directly via the BlackPill's USB-C port.

## 🛠️ Project Structure

- `src/`: Core logic files modernized for 32-bit C++.
    - `main.cpp`: Orchestrates the 50Hz control loop.
    - `Hexapod.cpp`: Manages gaits, body balancing, and timing.
    - `Leg.cpp`: Implements 3D Inverse and Forward Kinematics using `float` math.
- `lib/`: Hardware-specific drivers.
    - `PS2X_lib`: Optimized Hardware SPI driver for the wireless receiver.
- `test/`: Comprehensive regression suite verifying IK, gait, and servo bitstreams against a verified Golden Master.

## 🔌 Hardware Pinout (BlackPill)

Mapping of physical headers (Top = USB Port side).

| Left Hdr | Function | Bot Board  |      | Right Hdr | Function                 | Bot Board    | 
| :---     | :---     | :---       | :--- | :---      | :---                     | :---         | 
| **B12**  | -        | RS232-Tx   | | **5V**    | 5Vin                     | 5Vin         | 
| **B13**  | -        | RS232-Rx   | | **GND**   | GND                      | GND          | 
| **B14**  | -        | RS232-RTS  | | **3V3**   | n.c.                     | RESETn       | 
| **B15**  | n.c.     | GND        | | **B10**   | n.c.                     | 5Vin         | 
| **A8**   | -        | -          | | **B2**    | -                        | (GND/noPU)   | 
| **A9**   | -        | -          | | **B1**    | -                        | (LED-Y)      | 
| **A10**  | -        | -          | | **B0**    | -                        | (LED-B)      | 
| **A11**  | USB-DM   | -          | | **A7**    | **PS2_CMD** (SPI1)       | (LED-R/noPU) | 
| **A12**  | USB-DP   | -          | | **A6**    | **PS2_DAT** (SPI1+PU10k) | -            |
| **A15**  | -        | -          | | **A5**    | **PS2_CLK** (SPI1+PU10k) | -            |
| **B3**   | -        | -          | | **A4**    | **PS2_SEL** (SPI1)       | (SOUND)      |
| **B4**   | -        | -          | | **A3**    | **SSC_RX** (UART2)       | -            |
| **B5**   | -        | Vservo 4:1 | | **A2**    | **SSC_TX** (UART2)       | -            |
| **B6**   | -        | Vlogic 4:1 | | **A1**    | **SOUND**                | -            |
| **B7**   | -        |            | | **A0**    | -                        |              |
| **B8**   | -        |            | | **NRST**  | RESETn                   |              |
| **B9**   | -        |            | | **C15**   | -                        |              |
| **5V**   | 5Vin     |            | | **C14**   | -                        |              |
| **GND**  | GND      |            | | **C13**   | -                        |              |
| **3V3**  | 3V3out   |            | | **VBAT**  | Vbat                     |              |

## 🎮 Controller Mapping

| Button | Action |
| :--- | :--- |
| **Start** | Turn Robot On / Off |
| **Triangle** | Stand Up (65mm) / Sit Down (0mm) |
| **Square** | Toggle Balance Mode (On/Off) |
| **Circle** | Toggle Single Leg Mode |
| **Cross** | Toggle GP Player Mode (Sequence execution) |
| **L1** | Toggle Translate Mode |
| **L2** | Toggle Rotate Mode |
| **R1** | Toggle Double Leg Lift Height (80mm / 50mm) |
| **R2** | Toggle Double Travel Length / Start GP Sequence |
| **R3** | Toggle Walk Method (Mode 1 / Mode 2) |
| **Select** | Switch Gait / Leg / GP Sequence |
| **D-Pad Up/Down** | Manual Body Height Adjust (+/- 10mm) |
| **D-Pad L/R** | Manual Speed Adjust (+/- 50ms) |
| **L-Stick** | Walk / Translate / Rotate X-Z |
| **R-Stick** | Rotate Y (Yaw) / Shift Y |

## 🚀 Future Roadmap & Optimizations

1. **Direct PWM (Eliminate SSC-32):** Use the BlackPill's 18+ hardware timers to drive servos natively, removing serial latency and allowing high-frequency updates.
2. **NeoPixel Integration:** Use reserved pins **`PB0/PB1`** (TIM3) with PWM+DMA for non-blocking status LEDs/Eyes.
3. **Continuous Trajectories:** Upgrade the Gait Engine to use **Bézier curves** or **Cycloids** for organic, fluid movement instead of discrete steps.
4. **Sensor Fusion:** Add an **IMU (MPU-6050)** via I2C for autonomous body leveling and tilt compensation.
5. **Production Polish:** Optional removal of the microsecond profiler ('P' command) once feature development is finalized.
6. **Test Expansion:** Continue maintaining the **Dual-Validation** test suite (Legacy vs. Float Master) for all new features.

## 💻 Building and Flashing

This project uses **VSCode + PlatformIO**.

1. Install the **ST-STM32** platform in PlatformIO.
2. Connect your BlackPill via **ST-LINK v2** (for flashing) and **USB-C** (for Serial).
3. Select the `blackpill_f411ce` environment and click **Build** or **Upload**.
4. Use the **Serial Monitor** at **57600 baud** to access the Phoenix Monitor.

---
*Modernized with 🤖 Gemini CLI.*
