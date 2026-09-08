# Wine Must Punch Controller

**Satori Cellars - Cap Management Automation**

This project automates the wine must cap management process, simulating manual punch-downs while providing configurable time- and temperature-initiated actuation. The system is designed to run on an Arduino Mega 2560 and utilizes a non-blocking Finite State Machine (FSM) to maintain a highly responsive touchscreen interface during mechanical operations.

---

## Hardware Stack

* **Microcontroller:** Arduino Mega 2560
* **Display:** 8-bit parallel ILI9341 TFT display with resistive TouchScreen panel
* **Temperature Sensing:** Maxim/Dallas DS18B20 (OneWire)
* **Actuation:** 4 Actuator Solenoids (Relay/Driver controlled)

---

## Program Structure & Modular Architecture

The codebase is built using **PlatformIO** and the Atmel AVR framework, structured into isolated modules to separate hardware concerns from core business logic.

* **`main.cpp`**
The central entry point and main loop coordinator. It handles EEPROM state restoration, evaluates time and temperature logic thresholds, and triggers punch sequences. It serves as the bridge between the UI, sensors, and actuator modules.
* **`Actuators.h` / `Actuators.cpp**`
A non-blocking Finite State Machine (FSM) that governs the mechanical punch sequence. It manages the timing for actuator extension, retraction, dwell states, and the multi-cycle post-punch shake routine without relying on `delay()`.
* **`UI.h` / `UI.cpp**`
Isolates all display initialization, page drawing routines, touch screen polling, and button event handlers. It utilizes the `MCUFRIEND_kbv` and `Adafruit_GFX` libraries optimized with `-O3` build flags for maximum render speed.
* **`Sensors.h` / `Sensors.cpp**`
Manages OneWire/Dallas temperature polling. It handles sensor initialization, error checking, and tracks minimum/maximum temperature values across power cycles.
* **`Config.h`**
The global configuration header containing hardware pin definitions, LCD analog control lines, touch calibration constants, UI color macros, and default operational timings.

---

## Operational Design Considerations

### Non-Blocking FSM Architecture

To ensure the 8-bit ILI9341 TFT display remains responsive to touch inputs, the system strictly avoids blocking functions like `delay()`. The actuator punch and shake routines operate as a continuous state machine (`PUNCH_DOWN`, `PUNCH_UP`, `PUNCH_SHAKE_WAIT`, etc.) evaluated on every cycle of the main loop against `millis()` timers.

### Power Failure Resilience

The controller utilizes the onboard EEPROM to periodically save critical state variables such as interval settings, target temperatures, dwell times, and cycle counts. Upon startup, the system checks for unexpected reboots; if a power failure occurs mid-cycle while the system is armed, it automatically executes a recovery punch to prevent the must cap from drying out.

### Sequential Actuation & Safety

To manage mechanical load and power draw, the actuators fire sequentially rather than simultaneously. The `AllActuatorsUp()` safety mechanism ensures all solenoid pins are driven `LOW` whenever the system returns to an idle state, aborts a sequence, or triggers an emergency stop, guaranteeing the plungers fully retract.

### Thermal & Time Triggers

The core logic evaluates two primary triggers:

1. **Time:** A countdown interval (configurable via UI) that initiates a punch sequence when the interval expires.
2. **Temperature:** A threshold monitor that initiates a punch if the must temperature exceeds a set maximum, provided a configurable "dwell time" (cooldown period) has elapsed since the last thermal event.
