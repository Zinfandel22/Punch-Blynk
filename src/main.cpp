#include <Arduino.h>
#include "Config.h"
#include "UI.h"
#include <EEPROM.h>
#include "Sensors.h"
#include "Actuators.h"

extern MCUFRIEND_kbv tft;

void AllActuatorsUp();
void StartPunch();

// --- HARDWARE ---
uint16_t identifier;
byte PinOutputs[4] = {Sol1, Sol2, Sol3, Sol4};

// --- PERSISTENT SETTINGS ---
byte _days;
byte Index;
byte IntervalSet;
int TempSetPoint;
int SetTempDwellTime;
byte StrokeDownTime;
byte StrokeUpTime = 15;
byte SetTimeRep_UI;
byte SetTempRep_UI;

// --- RUNTIME MEASUREMENTS AND TIMERS ---
unsigned long updateMillis = millis();
unsigned long InfoAge;
unsigned long UpdateAge = millis();
float TempAct;
float TMax;
float TMin;
int TempDwellTime;
float Interval;
int Cycles;
unsigned long lastPunchCompleteTime = 0;

// --- UI AND CYCLE STATE ---
byte CurrentPage = 1;
String s_UpTime;
String s_PunchReason = "Reboot";
String s_OldPunchReason;
int cycleValues[] = {1, 3, 5, 10};
int CycleIndex = 0;
int CycleValue = cycleValues[CycleIndex];
byte SetPunchReps;
int completedCycles = 0;

// --- ACTUATOR STATE ---
bool PunchActive = false;
bool AutoCycleEnabled;
bool ManualCycleActive = false;
bool ManualCycleCompletionHandled = false;
unsigned long ShakeDelay = 10000;
unsigned long ShakeDepth_ms = 200;
int MaxShakeCycles = 4;

// --- LOOKUP TABLES ---
byte _IntervalSet[8] = {1, 2, 3, 4, 6, 8, 12, 24};
byte StartOffset[12] = {00, 05, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55};

/* --------------------------------------------------------------------------------------------------------*/
void setup(void)
{
  Serial.begin(115200);
  if (identifier == 0x0101 || identifier == 0x0000 || identifier == 0xFFFF)
  {
    identifier = 0x9341;
  }
  tft.begin(identifier);
  tft.setRotation(1);
  initDisplay();
  initSensors();
  actuators.begin();

  for (byte i = 0; i <= 3; i++)
  {
    pinMode(PinOutputs[i], OUTPUT);
  }

  if (EEPROM.read(20) == 255)
  {
    EEPROM.update(20, 16);
    EEPROM.update(0, 0);
    EEPROM.update(1, 4);
    EEPROM.update(2, 0);
    EEPROM.update(3, 0);
    EEPROM.update(4, 90);
    EEPROM.update(6, 4);
    EEPROM.update(7, 4);
    EEPROM.update(8, 30);
    EEPROM.update(9, 30);
    EEPROM.update(11, 3);
  }

  AutoCycleEnabled = EEPROM.read(0);
  Index = EEPROM.read(1);
  IntervalSet = _IntervalSet[Index];
  Interval = 1440 / _IntervalSet[Index];
  TMax = EEPROM.read(2);
  TMin = EEPROM.read(3);
  TempSetPoint = EEPROM.read(4);
  SetTimeRep_UI = EEPROM.read(6);
  SetTempRep_UI = EEPROM.read(7);
  StrokeDownTime = EEPROM.read(8);
  SetTempDwellTime = EEPROM.read(9);
  Cycles = EEPROM.read(10);
  _days = EEPROM.read(11);

  TempAct = GetTemp();
  drawMainScreen();

  if (EEPROM.read(5) == 1 && AutoCycleEnabled == 1)
  {
    s_PunchReason = "Power";
    SetPunchReps = SetTimeRep_UI;
    completedCycles = 0;
    StartPunch();
  }
  else
  {
    EEPROM.update(5, 0);
  }
}

void loop()
{
  ReadScreen();
  actuators.update();

  if (ManualCycleActive && actuators.getState() == PUNCH_SHAKE_WAIT)
  {
    ManualCycleCompletionHandled = true;
  }

  if (ManualCycleActive && !PunchActive && actuators.getState() == PUNCH_IDLE && ManualCycleCompletionHandled)
  {
    if (completedCycles < SetPunchReps)
    {
      completedCycles++;
      ManualCycleCompletionHandled = false;
      StartPunch();
    }
    else
    {
      ManualCycleActive = false;
      ManualCycleCompletionHandled = false;
      if (CurrentPage == 3)
      {
        drawManualScreen();
      }
    }
  }

  if (millis() - UpdateAge >= 60000)
  {
    EEPROM.update(5, millis() > 3600000);
    TempDwellTime += 1;
    UpdateAge = millis();
  }

  if ((TempAct >= TempSetPoint && TempDwellTime >= SetTempDwellTime) && !PunchActive)
  {
    s_PunchReason = "Temp";
    SetPunchReps = SetTempRep_UI;
    StartPunch();
    TempDwellTime = 0;
    Interval = 1440 / IntervalSet;
  }

  if (millis() - updateMillis >= 1000)
  {
    updateMillis = millis();
    if (AutoCycleEnabled == 1)
    {
      Interval -= 0.01666667;
      if (Interval <= 0.02)
      {
        s_PunchReason = "Time";
        Interval = 1440 / IntervalSet;
        TempDwellTime = 0;
        SetPunchReps = SetTimeRep_UI;
        StartPunch();
      }
      if (s_OldPunchReason != s_PunchReason)
      {
        s_OldPunchReason = s_PunchReason;
        tft.fillRect(235, 222, 85, 15, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(240, 222);
        tft.println(s_OldPunchReason);
      }
    }

    if (CurrentPage != 4)
    {
      updateTemp();
      if (CurrentPage == 1)
      {
        updateActuatorState();
      }
      if (AutoCycleEnabled == 1 && CurrentPage == 1)
      {
        updateInterval();
      }
    }

    if ((CurrentPage == 2 || CurrentPage == 4 || (CurrentPage == 3 && AutoCycleEnabled == 0)) && millis() - InfoAge >= 60000)
    {
      CurrentPage = 1;
      drawMainScreen();
    }
  }

  if (!PunchActive && actuators.getState() == PUNCH_IDLE)
  {
    AllActuatorsUp();
  }
}

String Uptime()
{
  int time_mins = (int)floor(millis() / 60000);
  int hours = time_mins / 60;
  int minutes = time_mins % 60;
  String _hour = String(hours);
  String _min = String(minutes);
  if (_min.length() == 1)
  {
    _min = "0" + _min;
  }
  s_UpTime = _hour + ":" + _min;
  return s_UpTime;
}

// --- ACTUATOR CONTROL HELPERS ---
void StartShake()
{
  actuators.forceShake();
}

void AbortPunch()
{
  Serial.println("Reset Punch!");
  ManualCycleActive = false;
  ManualCycleCompletionHandled = false;
  completedCycles = SetPunchReps;
  actuators.abortPunch();

  Serial.println("Running shake routine after abort...");
  actuators.abortAndShake();
}

void StartPunch()
{
  actuators.startPunch();
}

void AllActuatorsUp()
{
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(PinOutputs[i], LOW);
  }
}