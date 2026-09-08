#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>
#include "config.h"

enum PunchState {
    PUNCH_IDLE,
    PUNCH_DOWN,
    PUNCH_UP,
    PUNCH_SHAKE_WAIT,
    PUNCH_SHAKE_DOWN,
    PUNCH_SHAKE_UP
};

class Actuators {
public:
    void begin();
    void update();
    
void startPunch();
void forceShake(); 
void abortPunch();
void abortAndShake();
void emergencyStop();

    bool isRunning() const;
    PunchState getState() const;
    byte getCurrentActuator() const;

private:
    PunchState punchState = PUNCH_IDLE;
    
    // Timing & Tracking variables
    unsigned long punchStartTime = 0;
    unsigned long upStartTime = 0;
    unsigned long shakeStateStartTime = 0;
    
    byte currentPunchActuator = 0;
    
    // Non-blocking shake variables
    int currentShakeActuator = 0;
    int currentShakeCycle = 0;

    void setAllActuators(uint8_t state);
    void transitionTo(PunchState newState);
};

extern Actuators actuators;

#endif