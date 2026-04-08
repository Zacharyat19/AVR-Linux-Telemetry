# Raspberry Pi Server Telemetry System

## Overview
This project is a hardware-software bridge designed to provide physical telemetry and control for a background Linux daemon running on a Raspberry Pi 5. The system utilizes a C daemon to monitor processes via the Linux `/proc` filesystem and transmits state data over a CDC-ACM USB serial connection to an ATmega32U4 microcontroller.

The microcontroller serves as a real-time interface, providing LED status indicators and a dedicated hardware restart button.

## System Architecture

### Software (Host: Raspberry Pi 5)
* **Language:** C
* **Process Monitoring:** Direct traversal of the Linux virtual filesystem (`/proc/[pid]/cmdline`). This allows the daemon to identify specific Python virtual environment processes that traditional `pgrep` commands might miss.
* **Asynchronous I/O:** Utilizes the POSIX `select()` system call to monitor upstream serial inputs (buttons) without blocking the downstream telemetry polling loop.
* **Hardware Interface:** POSIX `termios` configured for 115200 baud, 8N1, non-canonical raw output to ensure bytes are sent exactly as intended without kernel-level formatting.

### Hardware (Target: ATmega32U4)
* **Firmware:** AVR C++ (Compiled via Arduino Core, LilyPad USB 8MHz profile).
* **Interface:** Native USB CDC-ACM.
* **Input Logic:** Debounced active-low tactile switches using internal pull-up resistors ($10k\Omega$ to $50k\Omega$).
* **Protocol:** A standardized 1-byte hex-offset protocol for scalable management:
    * **ON Command:** `0x10 + SlotID` (e.g., Slot 0 = `0x10`)
    * **OFF Command:** `0x00 + SlotID` (e.g., Slot 0 = `0x00`)

## Hardware Configuration
To ensure electrical stability and prevent flickering, the system is restricted to the first four physical LEDs and a single debounced input.

### Pin Mapping Table
| Component | Physical Label | Schematic Net | Arduino Pin | Role |
| :--- | :--- | :--- | :--- | :--- |
| **LED 0** | D1 | pb.4 | **8** | **Photo-Server Status** |
| **LED 1** | D2 | pb.5 | **9** | Available |
| **LED 2** | D3 | pb.6 | **10** | Available |
| **LED 3** | D4 | pb.7 | **11** | Available |
| **Button 1** | S4 | pd.7 | **6** | **Service Restart (Signal 'R')** |

## Project Structure
* `main.c`: The primary daemon loop. Handles serial initialization, telemetry evaluation scheduling, and asynchronous interrupt listening.
* `Display.h` / `Display.c`: The abstraction layer containing `/proc` parsing logic and POSIX serial hardware dispatch.
* `Flash.cpp`: The AVR C++ code flashed to the ATmega32U4, managing LED states and debounced button inputs.

## Challenges & Technical Insights

### 1. Architecture Pivot: FPGA to Microcontroller

The project initially targeted the Terasic DE10-Lite (Altera MAX 10 FPGA). However, the overhead of the Quartus toolchain and the complexity of implementing a custom UART IP core for a simple 1-byte payload introduced unnecessary friction. The architecture was pivoted to the ATmega32U4 to leverage its native USB CDC-ACM controller, allowing for direct, lightweight Linux integration.

### 2. Bootloader Discovery & Flashing

Getting the new ATmega32U4 board online presented immediate flashing issues. Because the 32U4 handles both USB communication and application execution on a single silicon die, a port conflict or serial hang would cause the board to drop off the host machine's USB tree entirely, preventing new firmware from being flashed.

* **The Solution:** Discovered and mapped the hardware reset sequence (double-tapping the reset pin) to manually halt execution and force the chip back into its native bootloader state. This exposed the hidden COM port and stabilized the deployment process using the LilyPad USB 8MHz profile.

### 3. The DTR Reset Loop

During initial testing with standard bash commands, the microcontroller would flash its LEDs and reset rather than holding the commanded state.

* **The Root Cause:** Opening a serial port in Linux toggles the Data Terminal Ready (DTR) line. The ATmega32U4 is hardwired to reset on a DTR drop.

* **The Solution:** The C daemon was structured to open the file descriptor exactly once at startup and hold it open indefinitely (`O_NOCTTY`), bypassing the reset loop entirely.

### 4. Serial Protocol Evolution: The Switch to ASCII/Hex
Initially, attempts to transmit raw multi-byte binary packets resulted in significant data loss and inconsistent LED states. This was due to the Linux `termios` driver interpreting certain binary values as software flow control signals (e.g., `XON/XOFF`). 
* **The Solution:** The protocol was pivoted to a simplified 1-byte mapping. By using specific ASCII characters (like `'R'`) and hex-offset values in a hardware-safe range, bypassing the kernel's terminal processing quirks.

### 5. ST7565R Initialization & Bias Overrides
Standard libraries (U8g2/U8x8) failed to initialize the LCD correctly, resulting in solid black bars or "garbage" pixels on the left edge.
* The Solution: Switched to an `SW_SPI` (Bit-bang) constructor to bypass core library mapping errors. Manually injected raw hex commands (`0x22` for resistor ratio and `0x81` for electronic volume) directly into the display controller via `sendF()` to match the specific voltage requirements of the custom PCB.
