#include <Arduino.h>
#include "secrets.h"
#include "config.h"
#include "UI.h"
#include "Sensors.h"
#include "Actuators.h"
#define BLYNK_PRINT Serial
#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define APP_DEBUG
#include "BlynkEdgent.h"

extern TFT_eSPI tft;
Preferences preferences;

void AllActuatorsUp();
void StartPunch();
String Uptime();

// --- HARDWARE ---
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
unsigned long lastBlynkUpdate = 0;

// --- UI AND CYCLE STATE ---
byte CurrentPage = 1;
byte Phase = 0;
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
byte StartOffset[13] = {00, 05, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};

static void saveByteSetting(byte address, byte value)
{
  switch (address)
  {
  case 0:
    preferences.putBool("autoCycle", value);
    break;
  case 1:
    preferences.putUChar("intervalIndex", value);
    break;
  case 4:
    preferences.putInt("tempSetPoint", value);
    break;
  case 6:
    preferences.putUChar("timeReps", value);
    break;
  case 7:
    preferences.putUChar("tempReps", value);
    break;
  case 8:
    preferences.putUChar("strokeDown", value);
    break;
  case 9:
    preferences.putInt("tempDwell", value);
    break;
  case 11:
    preferences.putUChar("startDelay", value);
    break;
  }
}

static bool shakeActive()
{
  const PunchState state = actuators.getState();
  return state == PUNCH_SHAKE_WAIT || state == PUNCH_SHAKE_DOWN || state == PUNCH_SHAKE_UP;
}

void publishBlynkState()
{
  Blynk.virtualWrite(V0, Uptime());
  Blynk.virtualWrite(V1, IntervalSet);
  Blynk.virtualWrite(V2, TempAct);
  Blynk.virtualWrite(V3, String(Interval, 2));
  Blynk.virtualWrite(V4, s_PunchReason);
  Blynk.virtualWrite(V5, StrokeDownTime);
  Blynk.virtualWrite(V6, StrokeUpTime);
  Blynk.virtualWrite(V7, SetTimeRep_UI);
  Blynk.virtualWrite(V8, SetTempRep_UI);
  Blynk.virtualWrite(V9, !AutoCycleEnabled);
  Blynk.virtualWrite(V10, Phase);
  Blynk.virtualWrite(V11, shakeActive());
  Blynk.virtualWrite(V12, 0);
  Blynk.virtualWrite(V13, AutoCycleEnabled);
  Blynk.virtualWrite(V14, SetTempDwellTime);
  Blynk.virtualWrite(V15, TMax);
  Blynk.virtualWrite(V16, TempSetPoint);
  Blynk.virtualWrite(V17, 0);
}

static void setIntervalFromBlynk(int requestedInterval)
{
  for (byte index = 0; index < sizeof(_IntervalSet); index++)
  {
    if (_IntervalSet[index] == requestedInterval)
    {
      Index = index;
      IntervalSet = _IntervalSet[Index];
      Interval = 1440 / IntervalSet + _days * 5;
      saveByteSetting(1, Index);
      return;
    }
  }
}

BLYNK_CONNECTED()
{
  Blynk.syncAll();
  publishBlynkState();
}

BLYNK_WRITE(V1)
{
  setIntervalFromBlynk(param.asInt());
}

BLYNK_WRITE(V5)
{
  StrokeDownTime = constrain(param.asInt(), 5, 90);
  StrokeDownTime = (StrokeDownTime / 5) * 5;
  saveByteSetting(8, StrokeDownTime);
}

BLYNK_WRITE(V6)
{
  StrokeUpTime = constrain(param.asInt(), 1, 60);
}

BLYNK_WRITE(V7)
{
  SetTimeRep_UI = constrain(param.asInt(), 1, 10);
  saveByteSetting(6, SetTimeRep_UI);
}

BLYNK_WRITE(V8)
{
  SetTempRep_UI = constrain(param.asInt(), 1, 10);
  saveByteSetting(7, SetTempRep_UI);
}

BLYNK_WRITE(V9)
{
  AutoCycleEnabled = !param.asInt();
  saveByteSetting(0, AutoCycleEnabled);
  if (!AutoCycleEnabled)
  {
    AbortPunch();
  }
}

BLYNK_WRITE(V10)
{
  Phase = constrain(param.asInt(), 0, 3);
  preferences.putUChar("phase", Phase);
  if (CurrentPage == 1)
  {
    drawMainScreen();
  }
}

BLYNK_WRITE(V11)
{
  if (param.asInt() && !PunchActive)
  {
    StartShake();
  }
}

BLYNK_WRITE(V13)
{
  AutoCycleEnabled = param.asInt();
  saveByteSetting(0, AutoCycleEnabled);
  if (!AutoCycleEnabled)
  {
    AbortPunch();
  }
}

BLYNK_WRITE(V14)
{
  SetTempDwellTime = constrain(param.asInt(), 0, 240);
  saveByteSetting(9, SetTempDwellTime);
}

BLYNK_WRITE(V16)
{
  TempSetPoint = constrain(param.asInt(), 50, 120);
  saveByteSetting(4, TempSetPoint);
}

BLYNK_WRITE(V17)
{
  if (param.asInt())
  {
    TMax = 0;
    TMin = 99;
    Cycles = 0;
    saveByteSetting(2, TMax);
    saveByteSetting(3, TMin);
    saveByteSetting(10, Cycles);
  }
}

/* --------------------------------------------------------------------------------------------------------*/
void setup(void)
{
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting Punch-Blynk");

  preferences.begin("settings", false);
  Serial.println("Initializing display");
  initDisplay();
  Serial.println("Display initialized");

  BlynkEdgent.begin();
  Serial.print("Blynk Edgent state: ");
  Serial.println((int)BlynkState::get());
  Serial.println("Blynk provisioning startup requested");

  initSensors();
  actuators.begin();

  for (byte i = 0; i <= 3; i++)
  {
    pinMode(PinOutputs[i], OUTPUT);
  }

  if (!preferences.getBool("initialized", false))
  {
    preferences.putBool("initialized", true);
    preferences.putBool("autoCycle", false);
    preferences.putUChar("intervalIndex", 4);
    preferences.putFloat("tempMax", 0.0f);
    preferences.putFloat("tempMin", 0.0f);
    preferences.putInt("tempSetPoint", 90);
    preferences.putUChar("timeReps", 4);
    preferences.putUChar("tempReps", 4);
    preferences.putUChar("strokeDown", 30);
    preferences.putInt("tempDwell", 30);
    preferences.putInt("cycles", 0);
    preferences.putUChar("startDelay", 3);
    preferences.putUChar("phase", 0);
    preferences.putBool("powerRecovery", false);
  }

  AutoCycleEnabled = preferences.getBool("autoCycle", false);
  Index = preferences.getUChar("intervalIndex", 4);
  Index = min(Index, (byte)7);
  IntervalSet = _IntervalSet[Index];
  Interval = 1440 / _IntervalSet[Index];
  TMax = preferences.getFloat("tempMax", 0.0f);
  TMin = preferences.getFloat("tempMin", 0.0f);
  TempSetPoint = preferences.getInt("tempSetPoint", 90);
  SetTimeRep_UI = preferences.getUChar("timeReps", 4);
  SetTempRep_UI = preferences.getUChar("tempReps", 4);
  StrokeDownTime = preferences.getUChar("strokeDown", 30);
  SetTempDwellTime = preferences.getInt("tempDwell", 30);
  Cycles = preferences.getInt("cycles", 0);
  _days = preferences.getUChar("startDelay", 3);
  Phase = preferences.getUChar("phase", 0);
  Phase = min(Phase, (byte)3);
  if (_days > 12)
  {
    _days = 0;
    preferences.putUChar("startDelay", _days);
  }

  TempAct = GetTemp();
  drawMainScreen();

  if (preferences.getBool("powerRecovery", false) && AutoCycleEnabled)
  {
    s_PunchReason = "Power";
    SetPunchReps = SetTimeRep_UI;
    completedCycles = 0;
    StartPunch();
  }
  else
  {
    preferences.putBool("powerRecovery", false);
  }
}

void loop()
{
  ReadScreen();
  BlynkEdgent.run();
  static bool provisioningReported = false;
  if (!provisioningReported && (BlynkState::is(MODE_WAIT_CONFIG) || BlynkState::is(MODE_CONFIGURING)))
  {
    Serial.print("Blynk provisioning AP: ");
    Serial.println(WiFi.softAPIP());
    provisioningReported = true;
  }
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
    preferences.putBool("powerRecovery", millis() > 3600000);
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
    publishBlynkState();
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