#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// --- EXTERNAL VARIABLES (Bridging to main or UI) ---
extern float TempAct;
extern float TMax;
extern float TMin;

// --- SENSOR FUNCTIONS ---
void initSensors();
float GetTemp();