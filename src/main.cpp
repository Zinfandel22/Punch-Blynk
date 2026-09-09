#include <Arduino.h>
#include "secrets.h"
#include "config.h"
#include "UI.h"
#include "Sensors.h"
#include "Actuators.h"
#include <time.h>
#define BLYNK_PRINT Serial
#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define APP_DEBUG
#include "BlynkEdgent.h"

extern TFT_eSPI tft;
Preferences preferences;
BlynkTimer applicationTimer;

void AllActuatorsUp();
void StartPunch();
String timerValue();
void updateCurrentTime();
static void handleManualCycleProgress();
static void persistRuntimeState();
static void evaluateAutomaticTriggers();
static void updatePeriodicDisplay();

// --- HARDWARE ---
byte PinOutputs[4] = {Sol1, Sol2, Sol3, Sol4};

// --- PERSISTENT SETTINGS ---
int TempSetPoint[4] = {90, 90, 90, 90};
int SetTempDwellTime;
byte StrokeDownTime;
byte StrokeUpTime = 15;
byte SetTimeRep_UI;
byte SetTempRep_UI;

// --- RUNTIME MEASUREMENTS AND TIMERS ---
unsigned long updateMillis = millis();
unsigned long lastRuntimePersistTime = millis();
float TempAct;
float TMax;
float TMin;
int TempDwellTime;
float Interval;
int Cycles;
unsigned long lastPunchCompleteTime = 0;
bool displayRefreshPending = false;
unsigned long displayRefreshDue = 0;
unsigned long calibrationCommandBlockedUntil = 0;

// --- UI AND CYCLE STATE ---
byte Phase = 0;
String s_PunchReason = "Reboot";
String BinName = "Bin";
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
byte CycleFrequency[4] = {1, 4, 6, 24};
byte CycleTimeDown[4] = {30, 30, 30, 30};
byte NumCyclesTime[4] = {4, 4, 4, 4};
byte NumCyclesTemp[4] = {4, 4, 4, 4};
byte TempDwell[4] = {30, 30, 30, 30};

void applyPhaseSettings(bool resetTimer)
{
  Phase = min(Phase, (byte)3);
  CycleFrequency[Phase] = constrain(CycleFrequency[Phase], 1, 24);
  StrokeDownTime = CycleTimeDown[Phase];
  SetTimeRep_UI = NumCyclesTime[Phase];
  SetTempRep_UI = NumCyclesTemp[Phase];
  SetTempDwellTime = TempDwell[Phase];
  if (resetTimer)
  {
    Interval = 60.0f * CycleFrequency[Phase];
  }
}

static void savePhaseSettings()
{
  preferences.putBytes("cycleFreq", CycleFrequency, sizeof(CycleFrequency));
  preferences.putBytes("cycleDown", CycleTimeDown, sizeof(CycleTimeDown));
  preferences.putBytes("numCyclesTime", NumCyclesTime, sizeof(NumCyclesTime));
  preferences.putBytes("numCyclesTemp", NumCyclesTemp, sizeof(NumCyclesTemp));
  preferences.putBytes("tempDwell", TempDwell, sizeof(TempDwell));
  preferences.putBytes("tempSetPoint", TempSetPoint, sizeof(TempSetPoint));
}

// Persist the running countdown so power cycles resume without waiting on Blynk
static void saveTimerState()
{
  preferences.putFloat("runInterval", Interval);
  preferences.putInt("runDwellTime", TempDwellTime);
}

static void saveByteSetting(byte address, byte value)
{
  switch (address)
  {
  case 0:
    preferences.putBool("autoCycle", value);
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
  }
}

static bool shakeActive()
{
  const PunchState state = actuators.getState();
  return state == PUNCH_SHAKE_WAIT || state == PUNCH_SHAKE_DOWN || state == PUNCH_SHAKE_UP;
}

void publishBlynkState()
{
  Blynk.virtualWrite(V0, String(TempDwellTime));
  Blynk.virtualWrite(V1, CycleFrequency[Phase]);
  Blynk.virtualWrite(V2, TempAct);
  Blynk.virtualWrite(V3, String(Interval, 2));
  Blynk.virtualWrite(V4, s_PunchReason);
  Blynk.virtualWrite(V5, StrokeDownTime);
  Blynk.virtualWrite(V6, StrokeUpTime);
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
  Blynk.virtualWrite(V19, BinName);
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

  initialized = true;
}

static void refreshDisplayForBlynk()
{
  displayRefreshPending = true;
  displayRefreshDue = millis() + 250;
}

static void drawPendingDisplay()
{
  if (!displayRefreshPending || millis() < displayRefreshDue)
    return;

  displayRefreshPending = false;
  drawMainScreen();
}

BLYNK_CONNECTED()
{
  calibrationCommandBlockedUntil = millis() + 3000;
  // Preferences are authoritative; publish local state instead of restoring cloud values.
  publishBlynkState();
}

BLYNK_WRITE(V1)
{
  CycleFrequency[Phase] = constrain(param.asInt(), 1, 24);
  savePhaseSettings();
  applyPhaseSettings(false);
  const float requestedInterval = 60.0f * CycleFrequency[Phase];
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
  applyPhaseSettings();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V6)
{
  StrokeUpTime = constrain(param.asInt(), 1, 60);
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V7)
{
  NumCyclesTime[Phase] = constrain(param.asInt(), 1, 10);
  savePhaseSettings();
  applyPhaseSettings();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V8)
{
  NumCyclesTemp[Phase] = constrain(param.asInt(), 1, 10);
  savePhaseSettings();
  applyPhaseSettings();
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
  applyPhaseSettings();
  publishBlynkState();
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V11)
{
  if (param.asInt())
  {
    if (!PunchActive)
    {
      StartShake();
    }
    Blynk.virtualWrite(V11, 0);
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
  refreshDisplayForBlynk();
}

BLYNK_WRITE(V14)
{
  TempDwell[Phase] = constrain(param.asInt(), 0, 240);
  savePhaseSettings();
  applyPhaseSettings();
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
    preferences.putFloat("tempMax", TMax);
    preferences.putFloat("tempMin", TMin);
    saveByteSetting(10, Cycles);
    preferences.putBool("powerRecovery", true);
    AbortPunch();
    refreshDisplayForBlynk();
    Blynk.virtualWrite(V17, 0);
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
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  applicationTimer.setInterval(1000L, publishBlynkTelemetry);
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
    preferences.putFloat("tempMax", 0.0f);
    preferences.putFloat("tempMin", 99.0f);
    preferences.putBytes("tempSetPoint", TempSetPoint, sizeof(TempSetPoint));
    preferences.putUChar("timeReps", 4);
    preferences.putUChar("tempReps", 4);
    preferences.putUChar("strokeDown", 30);
    preferences.putInt("tempDwell", 30);
    preferences.putInt("cycles", 0);
    preferences.putUChar("phase", 0);
    preferences.putString("binName", "Bin");
    preferences.putUChar("profileSchema", 4);
    savePhaseSettings();
    preferences.putBool("powerRecovery", false);
  }

  AutoCycleEnabled = preferences.getBool("autoCycle", false);
  TMax = preferences.getFloat("tempMax", 0.0f);
  TMin = preferences.getFloat("tempMin", 99.0f);
  if (TMin <= 0.0f)
  {
    TMin = 99.0f;
    preferences.putFloat("tempMin", TMin);
  }
  Cycles = preferences.getInt("cycles", 0);
  Phase = preferences.getUChar("phase", 0);
  BinName = preferences.getString("binName", "Bin");
  Phase = min(Phase, (byte)3);
  preferences.getBytes("cycleFreq", CycleFrequency, sizeof(CycleFrequency));
  preferences.getBytes("cycleDown", CycleTimeDown, sizeof(CycleTimeDown));
  preferences.getBytes("numCyclesTime", NumCyclesTime, sizeof(NumCyclesTime));
  preferences.getBytes("numCyclesTemp", NumCyclesTemp, sizeof(NumCyclesTemp));
  preferences.getBytes("tempDwell", TempDwell, sizeof(TempDwell));
  preferences.getBytes("tempSetPoint", TempSetPoint, sizeof(TempSetPoint));
  const byte profileSchema = preferences.getUChar("profileSchema", 0);
  const int legacyTempSetPoint = preferences.getInt("tempSetPoint", 90);
  if (profileSchema == 2)
  {
    const byte legacyFrequency[4] = {1, 4, 6, 24};
    for (byte profile = 0; profile < 4; profile++)
    {
      const byte legacyIndex = CycleFrequency[profile];
      if (legacyIndex < 8)
      {
        CycleFrequency[profile] = legacyFrequency[legacyIndex];
      }
    }
    preferences.putUChar("profileSchema", 3);
    savePhaseSettings();
  }
  if (profileSchema < 4)
  {
    for (byte profile = 0; profile < 4; profile++)
    {
      TempSetPoint[profile] = constrain(legacyTempSetPoint, 50, 120);
    }
    preferences.putUChar("profileSchema", 4);
    savePhaseSettings();
  }
  else if (profileSchema != 4)
  {
    preferences.putUChar("profileSchema", 4);
  }
  applyPhaseSettings(true);

  // Restore in-progress countdown/dwell instead of the freshly computed phase defaults
  if (preferences.isKey("runInterval"))
  {
    Interval = preferences.getFloat("runInterval", Interval);
  }
  TempDwellTime = preferences.getInt("runDwellTime", TempDwellTime);

  TempAct = GetTemp();
  refreshDisplayForBlynk();

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
  applicationTimer.run();
  drawPendingDisplay();
  static bool provisioningReported = false;
  if (!provisioningReported && (BlynkState::is(MODE_WAIT_CONFIG) || BlynkState::is(MODE_CONFIGURING)))
  {
    Serial.print("Blynk provisioning AP: ");
    Serial.println(WiFi.softAPIP());
    provisioningReported = true;
  }
  actuators.update();
  updateShakeButton();
  handleManualCycleProgress();
  persistRuntimeState();
  evaluateAutomaticTriggers();
  updatePeriodicDisplay();

  if (!PunchActive && actuators.getState() == PUNCH_IDLE)
  {
    AllActuatorsUp();
  }
}

static void handleManualCycleProgress()
{
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
    }
  }
}

static void persistRuntimeState()
{
  if (millis() - lastRuntimePersistTime >= 60000)
  {
    preferences.putBool("powerRecovery", millis() > 3600000);
    TempDwellTime += 1;
    lastRuntimePersistTime = millis();
    saveTimerState();
  }
}

static void evaluateAutomaticTriggers()
{
  if ((TempAct >= TempSetPoint[Phase] && TempDwellTime >= SetTempDwellTime) && !PunchActive)
  {
    s_PunchReason = "Temp";
    SetPunchReps = SetTempRep_UI;
    StartPunch();
    TempDwellTime = 0;
    Interval = 60.0f * CycleFrequency[Phase];
    saveTimerState();
  }
}

static void updatePeriodicDisplay()
{
  if (millis() - updateMillis >= 1000)
  {
    updateMillis = millis();
    if (AutoCycleEnabled == 1)
    {
      Interval -= 0.01666667;
      if (Interval <= 0.02)
      {
        s_PunchReason = "Time";
        Interval = 60.0f * CycleFrequency[Phase];
        TempDwellTime = 0;
        SetPunchReps = SetTimeRep_UI;
        StartPunch();
        saveTimerState();
      }
    }

    updateCurrentTime();
    updateTemp();
    updateActuatorState();
    if (AutoCycleEnabled == 1)
    {
      updateInterval();
    }

  }
}

String timerValue()
{
  long totalSeconds = max(0L, (long)(Interval * 60.0f));
  const int hours = totalSeconds / 3600;
  const int minutes = (totalSeconds % 3600) / 60;
  const int seconds = totalSeconds % 60;

  char timerText[12];
  snprintf(timerText, sizeof(timerText), "%02d:%02d:%02d",
           hours, minutes, seconds);
  return String(timerText);
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

BLYNK_WRITE(V19)
{
  BinName = param.asStr();
  preferences.putString("binName", BinName);
  updateBinName();
  refreshDisplayForBlynk();
}