# Photo Server Telemetry System

## Overview
This project is a hardware-software bridge designed to provide physical telemetry for a background Linux daemon running on a Raspberry Pi 5. It consists of a C daemon that monitors kernel processes via the `/proc` filesystem and transmits state data over a CDC-ACM USB serial connection to an ATmega32U4 microcontroller board.

The microcontroller serves as an interface with real-time LED status indicators.

## System Architecture

### Software (Host: Raspberry Pi 5)
* **Language:** C
* **Process Monitoring:** Direct traversal of the Linux virtual filesystem (`/proc`). This avoids the overhead of spawning external shell processes (like `pgrep` or `system()`), keeping CPU utilization near 0%.
* **Hardware Interface:** POSIX `termios`. The connection is configured for 115200 baud, 8N1, non-canonical raw output.

### Hardware (Target: ATmega32U4)
* **Firmware:** AVR C++ (Compiled via Arduino Core, LilyPad USB 8MHz profile)
* **Interface:** Native USB CDC-ACM (Programming Port J1)
* **Protocol:** Strict 1-byte binary payload (`0x01` for UP, `0x00` for DOWN).

## Hardware Pin Mapping
Due to the multiplexed nature of the ATmega32U4's Port B and the specific physical traces on the custom PCB, the standard Arduino digital pin numbering does not align sequentially with the physical LEDs. 

This project utilizes a calibrated mapping array to ensure a true physical "Left-to-Right" progression across the two LED banks.

| Physical Position | LED Label | ATmega32U4 Port | Arduino Digital Pin | Notes |
| :--- | :--- | :--- | :--- | :--- |
| 1 (Far Left) | D5 | PB0 | 17 | Shared with USB RX Interrupt |
| **2** | **D6** | **PB1** | **15** | **Primary Telemetry Status LED** |
| 3 | D7 | PB2 | 16 | |
| 4 | D8 | PB3 | 14 | |
| 5 | D1 | PB4 | 8 | |
| 6 | D2 | PB5 | 9 | |
| 7 | D3 | PB6 | 10 | |
| 8 (Far Right)| D4 | PB7 | 11 | |

*Note: Pin 17 (D5) is hardwired in the Arduino USB core to flash on serial receive (RX). Therefore, Pin 15 (D6) is utilized as the dedicated application-level status indicator to prevent visual collisions with background OS polling.*

## Project Structure
* `main.c`: The primary daemon loop. Handles initialization, evaluation scheduling, and graceful shutdown.
* `Display.h` / `Display.c`: The abstraction layer containing the `/proc` parsing logic and POSIX serial hardware dispatch.
* `Flash.cpp`: The AVR C++ code to be flashed to the ATmega32U4.

## Challenges & Troubleshooting

### 1. Architecture Pivot: FPGA to Microcontroller
The project initially targeted the Terasic DE10-Lite (Altera MAX 10 FPGA). However, the overhead of the Quartus toolchain and the complexity of implementing a custom UART IP core for a simple 1-byte payload introduced unnecessary friction. The architecture was pivoted to the ATmega32U4 to leverage its native USB CDC-ACM controller, allowing for direct, lightweight Linux integration.

### 2. Bootloader Discovery & Flashing
Getting the new ATmega32U4 board online presented immediate flashing issues. Because the 32U4 handles both USB communication and application execution on a single silicon die, a port conflict or serial hang would cause the board to drop off the host machine's USB tree entirely, preventing new firmware from being flashed.
* **The Solution:** Discovered and mapped the hardware reset sequence (double-tapping the reset pin) to manually halt execution and force the chip back into its native bootloader state. This exposed the hidden COM port and stabilized the deployment process using the LilyPad USB 8MHz profile.

### 3. The DTR Reset Loop
During initial testing with standard bash commands, the microcontroller would flash its LEDs and reset rather than holding the commanded state.
* **The Root Cause:** Opening a serial port in Linux toggles the Data Terminal Ready (DTR) line. The ATmega32U4 is hardwired to reset on a DTR drop.
* **The Solution:** The C daemon was structured to open the file descriptor exactly once at startup and hold it open indefinitely (`O_NOCTTY`), bypassing the reset loop entirely.

## Future Roadmap: Bi-Directional Command & Control

Currently, the telemetry bridge operates as a simplex system (Host $\rightarrow$ MCU). The next architectural phase will upgrade the CDC-ACM serial link to support full duplex communication, transforming the ATmega32U4 from a passive display into an active hardware controller. 

The primary goal is to map the physical pushbuttons on the microcontroller board to system-level process controls on the Raspberry Pi.

### Implementation Roadmap

1. **Hardware Debouncing & Polling (MCU):** Update the AVR C++ firmware to monitor the onboard pushbuttons

2. **Upstream Protocol (MCU $\rightarrow$ Host):**
   Expand the 1-byte protocol to include upstream command payloads

3. **Asynchronous Serial Reads (Host):**
   Upgrade the C daemon's telemetry loop to listen for incoming bytes.

4. **Kernel-Level Process Control (Host):**
   Upon receiving a valid upstream command from the hardware, the C daemon will interface with the OS to restart the target services.
