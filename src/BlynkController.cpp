#include <Arduino.h>
#include "secrets.h"
#include "config.h"
#define BLYNK_PRINT Serial
#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define APP_DEBUG
#include "BlynkController.h"
#include "BlynkEdgent.h"
#include "UI.h"
#include "Sensors.h"
#include "Actuators.h"

extern Preferences preferences;
extern int TempSetPoint[4];
extern int SetTempDwellTime;
extern byte StrokeDownTime;
extern byte SetTimeRep_UI;
extern byte SetTempRep_UI;
extern float TempAct;
extern float TMax;
extern int TempDwellTime;
extern float Interval;
extern int Cycles;
extern byte Phase;
extern String s_PunchReason;
extern String BinName;
extern int CycleValue;
extern bool AutoCycleEnabled;
extern bool PunchActive;
extern bool ManualCycleActive;
extern bool ManualCycleCompletionHandled;
extern unsigned long calibrationCommandBlockedUntil;
extern byte PhaseIntervalHours[];
extern byte CycleTimeDown[];
extern byte NumCyclesTime[];
extern byte NumCyclesTemp[];
extern byte TempDwell[];

extern void savePhaseSettings();
extern void saveByteSetting(byte address, byte value);
extern void saveTimerState();
extern String timerValue();
extern void StartPunch();
extern void StartShake();
extern void AbortPunch();

static BlynkTimer applicationTimer;
static bool displayRefreshPending = false;
static unsigned long displayRefreshDue = 0;
unsigned long calibrationCommandBlockedUntil = 0;

static bool shakeActive()
{
  const PunchState state = actuators.getState();
  return state == PUNCH_SHAKE_WAIT || state == PUNCH_SHAKE_DOWN || state == PUNCH_SHAKE_UP;
}

static void refreshDisplayForBlynk()
{
  displayRefreshPending = true;
  displayRefreshDue = millis() + 250;
}

void requestBlynkDisplayRefresh()
{
  refreshDisplayForBlynk();
}

static void drawPendingDisplay()
{
  if (!displayRefreshPending || millis() < displayRefreshDue)
    return;

  displayRefreshPending = false;
  drawMainScreen();
}

void publishBlynkState()
{
  Blynk.virtualWrite(V0, String(TempDwellTime));
  Blynk.virtualWrite(V1, PhaseIntervalHours[Phase]);
  Blynk.virtualWrite(V2, TempAct);
  Blynk.virtualWrite(V3, String(Interval, 2));
  Blynk.virtualWrite(V4, s_PunchReason);
  Blynk.virtualWrite(V5, StrokeDownTime);
  Blynk.virtualWrite(V6, CycleValue);
  Blynk.virtualWrite(V7, SetTimeRep_UI);
  Blynk.virtualWrite(V8, SetTempRep_UI);
  Blynk.virtualWrite(V9, 0);
  Blynk.virtualWrite(V10, Phase);
  Blynk.virtualWrite(V11, shakeActive());
  Blynk.virtualWrite(V12, timerValue());
  Blynk.virtualWrite(V13, AutoCycleEnabled);
  Blynk.virtualWrite(V14, SetTempDwellTime);
  Blynk.virtualWrite(V15, TMax);
  Blynk.virtualWrite(V16, TempSetPoint[Phase]);
  Blynk.virtualWrite(V17, 0);
  Blynk.virtualWrite(V18, Cycles);
  Blynk.virtualWrite(V19, BinName);
  Blynk.virtualWrite(V20, actuatorCloudStateLabel());
}

static void publishBlynkTelemetry()
{
  static bool initialized = false;
  static String lastTimerValue;
  static String lastTempDwellValue;
  static float lastTemperature;
  static float lastInterval;
  static String lastPunchReason;
  static bool lastShakeState;
  static float lastMaximumTemperature;
  static int lastCycles;
  static int lastCycleValue;

  const String currentTimerValue = timerValue();
  const String currentTempDwellValue = String(TempDwellTime);
  const bool currentShakeState = shakeActive();

  if (!initialized || currentTimerValue != lastTimerValue)
  {
    Blynk.virtualWrite(V12, currentTimerValue);
    lastTimerValue = currentTimerValue;
  }
  if (!initialized || currentTempDwellValue != lastTempDwellValue)
  {
    Blynk.virtualWrite(V0, currentTempDwellValue);
    lastTempDwellValue = currentTempDwellValue;
  }
  if (!initialized || TempAct != lastTemperature)
  {
    Blynk.virtualWrite(V2, TempAct);
    lastTemperature = TempAct;
  }
  if (!initialized || Interval != lastInterval)
  {
    Blynk.virtualWrite(V3, String(Interval, 2));
    lastInterval = Interval;
  }
  if (!initialized || s_PunchReason != lastPunchReason)
  {
    Blynk.virtualWrite(V4, s_PunchReason);
    lastPunchReason = s_PunchReason;
  }
  if (!initialized || currentShakeState != lastShakeState)
  {
    Blynk.virtualWrite(V11, currentShakeState);
    lastShakeState = currentShakeState;
  }
  if (!initialized || TMax != lastMaximumTemperature)
  {
    Blynk.virtualWrite(V15, TMax);
    lastMaximumTemperature = TMax;
  }
  if (!initialized || Cycles != lastCycles)
  {
    Blynk.virtualWrite(V18, Cycles);
    lastCycles = Cycles;
  }
  if (!initialized || CycleValue != lastCycleValue)
  {
    Blynk.virtualWrite(V6, CycleValue);
    lastCycleValue = CycleValue;
  }

  initialized = true;
}

void publishActuatorStateIfChanged()
{
  static String lastActuatorState;
  const String currentActuatorState = actuatorCloudStateLabel();
  if (currentActuatorState == lastActuatorState)
    return;

  Blynk.virtualWrite(V20, currentActuatorState);
  lastActuatorState = currentActuatorState;
}

void beginBlynkController()
{
  applicationTimer.setInterval(1000L, publishBlynkTelemetry);
}

void scheduleBlynkTimer(unsigned long interval, void (*callback)())
{
  applicationTimer.setInterval(interval, callback);
}

void beginBlynkEdgent()
{
  BlynkEdgent.begin();
}

bool blynkProvisioningActive()
{
  return BlynkState::is(MODE_WAIT_CONFIG) || BlynkState::is(MODE_CONFIGURING);
}

void publishCycleValue()
{
  Blynk.virtualWrite(V6, CycleValue);
}

void runBlynkController()
{
  BlynkEdgent.run();
  applicationTimer.run();
  drawPendingDisplay();
}

BLYNK_CONNECTED()
{
  calibrationCommandBlockedUntil = millis() + 3000;
  publishBlynkState();
}

BLYNK_WRITE(V1)
{
  PhaseIntervalHours[Phase] = constrain(param.asInt(), 1, 24);
  savePhaseSettings();
  applyPhaseSettings(false);
  const float requestedInterval = 60.0f * PhaseIntervalHours[Phase];
  if (Interval > requestedInterval)
  {
    Interval = requestedInterval;
    saveTimerState();
  }
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V5)
{
  CycleTimeDown[Phase] = constrain(param.asInt(), 5, 90);
  CycleTimeDown[Phase] = (CycleTimeDown[Phase] / 5) * 5;
  savePhaseSettings();
  applyPhaseSettings(false);
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V6)
{
  CycleValue = constrain(param.asInt(), 0, 5);
  if (CycleValue == 0)
  {
    ManualCycleActive = false;
    ManualCycleCompletionHandled = false;
    resetCycleButtonDisplay();
  }
  else if (!PunchActive)
  {
    ManualCycleActive = true;
    s_PunchReason = "Manual";
    TempDwellTime = 0;
    preferences.putInt("runDwellTime", TempDwellTime);
    ManualCycleCompletionHandled = false;
    StartPunch();
  }
  if (CycleValue > 0)
    resetCycleButtonDisplay();

  Blynk.virtualWrite(V6, CycleValue);
  publishBlynkState();
}

BLYNK_WRITE(V7)
{
  NumCyclesTime[Phase] = constrain(param.asInt(), 1, 10);
  savePhaseSettings();
  applyPhaseSettings(false);
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V8)
{
  NumCyclesTemp[Phase] = constrain(param.asInt(), 1, 10);
  savePhaseSettings();
  applyPhaseSettings(false);
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V9)
{
  if (param.asInt() == 1)
  {
    if (millis() < calibrationCommandBlockedUntil)
    {
      Blynk.virtualWrite(V9, 0);
      return;
    }
    resetTouchCalibration();
    refreshDisplayForBlynk();
    Blynk.virtualWrite(V9, 0);
  }
}

BLYNK_WRITE(V10)
{
  Phase = constrain(param.asInt(), 0, 3);
  preferences.putUChar("phase", Phase);
  applyPhaseSettingsPreservingTimer();
  publishBlynkState();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V11)
{
  if (param.asInt())
  {
    if (!PunchActive)
      StartShake();
    Blynk.virtualWrite(V11, 0);
  }
}

BLYNK_WRITE(V13)
{
  AutoCycleEnabled = param.asInt();
  saveByteSetting(0, AutoCycleEnabled);
  if (!AutoCycleEnabled)
    AbortPunch();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V14)
{
  TempDwell[Phase] = constrain(param.asInt(), 0, 240);
  savePhaseSettings();
  applyPhaseSettings(false);
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V16)
{
  TempSetPoint[Phase] = constrain(param.asInt(), 50, 120);
  savePhaseSettings();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V17)
{
  if (param.asInt())
  {
    TMax = 0;
    TMin = 99;
    Cycles = 0;
    TempDwellTime = 0;
    preferences.putFloat("tempMax", TMax);
    preferences.putFloat("tempMin", TMin);
    preferences.putInt("cycles", Cycles);
    saveTimerState();
    preferences.putBool("powerRecovery", true);
    AbortPunch();
    publishBlynkState();
    refreshDisplayForBlynk();
    Blynk.virtualWrite(V17, 0);
  }
}

BLYNK_WRITE(V19)
{
  BinName = param.asStr();
  preferences.putString("binName", BinName);
  updateBinName();
  refreshDisplayForBlynk();
}
