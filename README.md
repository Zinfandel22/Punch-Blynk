# Wine Must Punch Controller

**Satori Cellars - Cap Management Automation**

This project automates the wine must cap management process, simulating manual punch-downs while providing configurable time- and temperature-initiated actuation. The system is designed to run on a Wemos LOLIN Lite ESP32 and utilizes a non-blocking Finite State Machine (FSM) to maintain a highly responsive touchscreen interface during mechanical operations.

---

## Hardware Stack

* **Microcontroller:** Wemos LOLIN Lite ESP32
* **Display:** ILI9488 TFT display with resistive touch panel, driven by TFT_eSPI
* **Temperature Sensing:** Maxim/Dallas DS18B20 (OneWire)
* **Actuation:** 4 Actuator Solenoids (Relay/Driver controlled)

---

## Program Structure & Modular Architecture

The codebase is built using **PlatformIO** and the Arduino ESP32 framework. Hardware-specific code is kept in modules, while `main.cpp` coordinates runtime state, persistence, Blynk input, and the actuator workflow.

* **`main.cpp`**
The central entry point and main loop coordinator. It restores Preferences state, handles Blynk virtual-pin commands, evaluates time and temperature triggers, and coordinates the UI, sensors, and actuator modules.
* **`Actuators.h` / `Actuators.cpp**`
The non-blocking Finite State Machine (FSM) that governs actuator extension, retraction, dwell states, and the post-punch shake routine.
* **`UI.h` / `UI.cpp**`
Contains display initialization, touch calibration, page drawing, screen polling, and local button handlers. It uses TFT_eSPI and stores touch calibration separately from application settings.
* **`Sensors.h` / `Sensors.cpp**`
Manages OneWire/Dallas temperature polling and tracks minimum and maximum temperatures.
* **`Config.h`**
Contains hardware pin definitions, display configuration, UI colors, and shared constants.

### Runtime Loop Order

Each pass through `loop()` follows this order:

1. Poll the local touchscreen and service Blynk Edgent.
2. Run scheduled Blynk telemetry and deferred display redraws.
3. Advance the actuator FSM.
4. Handle screen timeouts and manual-cycle progress.
5. Persist runtime state and evaluate automatic time/temperature triggers.
6. Refresh the clock, temperature, actuator state, and countdown once per second.
7. Force all actuators to the safe retracted state when idle.

---

## Operational Design Considerations

### Non-Blocking FSM Architecture

To keep the ILI9488 TFT display responsive to touch inputs, the actuator punch and shake routines operate as a continuous state machine (`PUNCH_DOWN`, `PUNCH_UP`, `PUNCH_SHAKE_WAIT`, etc.) evaluated on every cycle of the main loop against `millis()` timers.

### Power Failure Resilience

The controller uses the ESP32 `Preferences` library instead of EEPROM. Application settings are stored in the `settings` namespace, while TFT touch calibration is stored in the `touch` namespace. The running countdown and temperature dwell state are periodically saved so an armed controller can resume after power loss without depending on Blynk connectivity.

### Sequential Actuation & Safety

To manage mechanical load and power draw, the actuators fire sequentially rather than simultaneously. The `AllActuatorsUp()` safety mechanism ensures all solenoid pins are driven `LOW` whenever the system returns to an idle state, aborts a sequence, or triggers an emergency stop, guaranteeing the plungers fully retract.

### Thermal & Time Triggers

The core logic evaluates two primary triggers:

1. **Time:** A countdown interval (configurable via UI) that initiates a punch sequence when the interval expires.
2. **Temperature:** A threshold monitor that initiates a punch if the must temperature exceeds a set maximum, provided a configurable "dwell time" (cooldown period) has elapsed since the last thermal event.
