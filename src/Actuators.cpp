#include "Actuators.h"

// External variables from main.cpp
extern byte PinOutputs[4];
extern byte StrokeDownTime;
extern unsigned long ShakeDelay;
extern unsigned long ShakeDepth_ms;
extern int MaxShakeCycles;
extern int Cycles;
extern bool PunchActive;

// Extern reference to update the global maintained in main.cpp
extern unsigned long lastPunchCompleteTime; 

static constexpr unsigned long StrokeUpTimeMs = 30000;

void Actuators::begin() {
    for (byte i = 0; i < 4; i++) {
        pinMode(PinOutputs[i], OUTPUT);
        digitalWrite(PinOutputs[i], LOW);
    }
    punchState = PUNCH_IDLE;
    PunchActive = false;
}

void Actuators::update() {
    unsigned long now = millis();

    switch (punchState) {
        case PUNCH_IDLE:
            break;

        case PUNCH_DOWN:
            digitalWrite(PinOutputs[currentPunchActuator], HIGH);
            if (now - punchStartTime >= (unsigned long)StrokeDownTime * 1000) {
                digitalWrite(PinOutputs[currentPunchActuator], LOW);
                upStartTime = now;
                transitionTo(PUNCH_UP);
            }
            break;

        case PUNCH_UP:
            if (now - upStartTime >= StrokeUpTimeMs) {
                currentPunchActuator++;
                if (currentPunchActuator < 4) {
                    punchStartTime = now;
                    transitionTo(PUNCH_DOWN);
                } else {
                    lastPunchCompleteTime = now; // Update global for UI
                    Cycles += 1;
                    preferences.putInt("cycles", Cycles);
                    transitionTo(PUNCH_SHAKE_WAIT);
                }
            }
            break;

        case PUNCH_SHAKE_WAIT:
            if (now - lastPunchCompleteTime >= ShakeDelay) {
                forceShake(); 
            }
            break;

        case PUNCH_SHAKE_DOWN:
            if (now - shakeStateStartTime >= ShakeDepth_ms) {
                digitalWrite(PinOutputs[currentShakeActuator], LOW); 
                shakeStateStartTime = now;
                transitionTo(PUNCH_SHAKE_UP);
            }
            break;

        case PUNCH_SHAKE_UP:
            if (now - shakeStateStartTime >= ShakeDepth_ms) {
                currentShakeCycle++;
                if (currentShakeCycle < MaxShakeCycles) {
                    digitalWrite(PinOutputs[currentShakeActuator], HIGH);
                    shakeStateStartTime = now;
                    transitionTo(PUNCH_SHAKE_DOWN);
                } else {
                    currentShakeCycle = 0;
                    currentShakeActuator++;
                    if (currentShakeActuator < 4) {
                        digitalWrite(PinOutputs[currentShakeActuator], HIGH);
                        shakeStateStartTime = now;
                        transitionTo(PUNCH_SHAKE_DOWN);
                    } else {
                        transitionTo(PUNCH_IDLE);
                        PunchActive = false;
                        setAllActuators(LOW);
                        lastPunchCompleteTime = millis(); // Update global for UI
                    }
                }
            }
            break;
    }
}

void Actuators::startPunch() {
    if (PunchActive) return;
    PunchActive = true;
    currentPunchActuator = 0;
    punchStartTime = millis();
    transitionTo(PUNCH_DOWN);
}

void Actuators::forceShake() {
    currentShakeActuator = 0;
    currentShakeCycle = 0;
    PunchActive = true;
    shakeStateStartTime = millis();
    digitalWrite(PinOutputs[currentShakeActuator], HIGH);
    transitionTo(PUNCH_SHAKE_DOWN);
}

void Actuators::abortPunch() {
    setAllActuators(LOW);
    PunchActive = false;
    transitionTo(PUNCH_IDLE);
    lastPunchCompleteTime = millis();
}

void Actuators::emergencyStop() {
    setAllActuators(LOW);
    PunchActive = false;
    transitionTo(PUNCH_IDLE);
}

bool Actuators::isRunning() const {
    return PunchActive;
}

PunchState Actuators::getState() const {
    return punchState;
}

byte Actuators::getCurrentActuator() const {
    return currentPunchActuator;
}

void Actuators::setAllActuators(uint8_t state) {
    for (byte i = 0; i < 4; i++) {
        digitalWrite(PinOutputs[i], state);
    }
}

void Actuators::transitionTo(PunchState newState) {
    punchState = newState;
}

void Actuators::abortAndShake() {
    Serial.println("Sequence aborted. Retracting all actuators and waiting for mandatory shake...");
    setAllActuators(LOW);             // Ensure all pins drop to retract plungers
    PunchActive = true;               // Keep FSM active so it continues to process
    lastPunchCompleteTime = millis(); // Reset the timer for the delay
    transitionTo(PUNCH_SHAKE_WAIT);   // Enter the standard wait phase to allow physical retraction
}

Actuators actuators;