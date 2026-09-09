#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

class UiButton
{
public:
	void initButton(TFT_eSPI *display, int16_t x, int16_t y, uint16_t width,
									uint16_t height, uint16_t outline, uint16_t fill,
									uint16_t textColor, char *label, uint8_t textSize);
	void drawButton(bool inverted = false);
	bool contains(int16_t x, int16_t y) const;

private:
	TFT_eSPI *display = nullptr;
	int16_t centerX = 0;
	int16_t centerY = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	uint16_t outline = 0;
	uint16_t fill = 0;
	uint16_t textColor = 0;
	char *label = nullptr;
	uint8_t textSize = 1;
};

// --- EXTERN VARIABLES (Bridging to main.cpp) ---
extern float TempAct, TMax, TMin, Interval;
extern int TempSetPoint[], Cycles, SetTempDwellTime, TempDwellTime, completedCycles;
extern byte StrokeDownTime;
extern byte CycleFrequency[];
extern byte SetTimeRep_UI, SetTempRep_UI, SetPunchReps;
extern String s_PunchReason, BinName;
extern bool AutoCycleEnabled, PunchActive, ManualCycleActive;
extern bool ManualCycleCompletionHandled;
extern int cycleValues[];
extern int CycleIndex, CycleValue;
extern byte Phase;
extern TFT_eSPI tft;
void publishBlynkState();
void applyPhaseSettings(bool resetTimer = true);

// --- EXTERNAL FUNCTIONS (Defined elsewhere, called by UI) ---
float GetTemp();
void AbortPunch();
void StartPunch();
void StartShake();

// --- UI FUNCTION PROTOTYPES ---
void initDisplay();
void resetTouchCalibration();
void ReadScreen();
void drawMainScreen();
void updateInterval();
void updateTemp();
void updateBinName();
void updateActuatorState();
void updateShakeButton();
void UpdateSetInterval();
bool ScreenTouched();

// Touch Handlers
void handleTempAdjust();
void handleRunStop();
void handleManualCycle();
void handleInfoPage();
void handleProfileButton();