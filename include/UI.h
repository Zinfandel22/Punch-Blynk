#pragma once
#include <Arduino.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <Adafruit_GFX.h>
#include "Config.h"

// --- EXTERN VARIABLES (Bridging to main.cpp) ---
extern unsigned long InfoAge;
extern float TempAct, TMax, TMin, Interval;
extern int TempSetPoint, Cycles, SetTempDwellTime, TempDwellTime, completedCycles;
extern byte Index, IntervalSet, _days, StrokeDownTime;
extern byte SetTimeRep_UI, SetTempRep_UI, SetPunchReps;
extern String s_UpTime, s_PunchReason, s_OldPunchReason;
extern bool CycleFlag, PunchActive;
extern int cycleValues[];
extern int CycleIndex, CycleValue;
extern byte _IntervalSet[];
extern byte StartOffset[];
extern byte CurrentPage;

// --- EXTERNAL FUNCTIONS (Defined elsewhere, called by UI) ---
float GetTemp();
void AbortPunch();
void StartPunch();
void StartShake();
String Uptime();

// --- UI FUNCTION PROTOTYPES ---
void initDisplay();
void ReadScreen();
void drawMainScreen();
void drawInfoScreen();
void drawManualScreen();
void drawConfigScreen();
void updateInterval();
void updateTemp();
void UpdateSetInterval();
void drawhomeicon();
bool ScreenTouched();

// Touch Handlers
void handleTempAdjust();
void handleIntervalAdjust();
void handleRunStop();
void handleManualCycle();
void handleInfoPage();
void handleManualCycleActions();
void handleStatusReset();
void handleOffsetAdjust();
void handleConfigAdjustments();
void handleHomeIcon();