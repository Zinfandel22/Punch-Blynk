#include <Arduino.h>
#include "secrets.h"
#include "config.h"
#include "UI.h"
#include "Sensors.h"
#include "Actuators.h"
#include "BlynkController.h"
#include <WiFi.h>
#include <time.h>

extern TFT_eSPI tft;
Preferences preferences;

void AllActuatorsUp();
void StartPunch();
String timerValue();
void updateCurrentTime();
void saveTimerState();
static void handleManualCycleProgress();
static void persistRuntimeState();
static void evaluateAutomaticTriggers();
void updatePeriodicDisplay();

// --- HARDWARE ---
byte PinOutputs[4] = {Sol1, Sol2, Sol3, Sol4};

// --- PERSISTENT SETTINGS ---
int TempSetPoint[4] = {90, 90, 90, 90};
int SetTempDwellTime;
byte StrokeDownTime;
byte SetTimeRep_UI;
byte SetTempRep_UI;

// --- RUNTIME MEASUREMENTS AND TIMERS ---
unsigned long lastRuntimePersistTime = millis();
float TempAct;
float TMax;
float TMin;
int TempDwellTime;
float Interval;
int Cycles;
unsigned long lastPunchCompleteTime = 0;

// --- UI AND CYCLE STATE ---
byte Phase = 0;
String s_PunchReason = "Reboot";
String BinName = "Bin";
int CycleValue = 0;
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

// --- PHASE DEFAULTS ---
byte PhaseIntervalHours[4] = {1, 4, 6, 24};
byte CycleTimeDown[4] = {30, 30, 30, 30};
byte NumCyclesTime[4] = {4, 4, 4, 4};
byte NumCyclesTemp[4] = {4, 4, 4, 4};
byte TempDwell[4] = {30, 30, 30, 30};

void applyPhaseSettings(bool resetTimer)
{
  Phase = min(Phase, (byte)3);
  PhaseIntervalHours[Phase] = constrain(PhaseIntervalHours[Phase], 1, 24);
  StrokeDownTime = CycleTimeDown[Phase];
  SetTimeRep_UI = NumCyclesTime[Phase];
  SetTempRep_UI = NumCyclesTemp[Phase];
  SetTempDwellTime = TempDwell[Phase];
  if (resetTimer)
  {
    Interval = 60.0f * PhaseIntervalHours[Phase];
  }
}

void applyPhaseSettingsPreservingTimer()
{
  applyPhaseSettings(false);
  const float requestedInterval = 60.0f * PhaseIntervalHours[Phase];
  if (Interval > requestedInterval)
  {
    Interval = requestedInterval;
    saveTimerState();
  }
}

void savePhaseSettings()
{
  preferences.putBytes("cycleFreq", PhaseIntervalHours, sizeof(PhaseIntervalHours));
  preferences.putBytes("cycleDown", CycleTimeDown, sizeof(CycleTimeDown));
  preferences.putBytes("numCyclesTime", NumCyclesTime, sizeof(NumCyclesTime));
  preferences.putBytes("numCyclesTemp", NumCyclesTemp, sizeof(NumCyclesTemp));
  preferences.putBytes("tempDwell", TempDwell, sizeof(TempDwell));
  preferences.putBytes("tempSetPoint", TempSetPoint, sizeof(TempSetPoint));
}

// Persist the running countdown so power cycles resume without waiting on Blynk
void saveTimerState()
{
  preferences.putFloat("runInterval", Interval);
  preferences.putInt("runDwellTime", TempDwellTime);
}

void saveByteSetting(byte address, byte value)
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

  beginBlynkEdgent();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  beginBlynkController();
  scheduleBlynkTimer(1000L, updatePeriodicDisplay);
  Serial.print("Blynk Edgent state: ");
  Serial.println("Blynk Edgent startup requested");
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
  preferences.getBytes("cycleFreq", PhaseIntervalHours, sizeof(PhaseIntervalHours));
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
      const byte legacyIndex = PhaseIntervalHours[profile];
      if (legacyIndex < 8)
      {
        PhaseIntervalHours[profile] = legacyFrequency[legacyIndex];
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
  requestBlynkDisplayRefresh();

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
  static bool provisioningReported = false;
  if (!provisioningReported && blynkProvisioningActive())
  {
    Serial.print("Blynk provisioning AP: ");
    Serial.println(WiFi.softAPIP());
    provisioningReported = true;
  }
  actuators.update();
  runBlynkController();
  publishActuatorStateIfChanged();
  updateShakeButton();
  updateCycleButton();
  handleManualCycleProgress();
  persistRuntimeState();
  evaluateAutomaticTriggers();

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
    if (CycleValue > 0)
    {
      CycleValue--;
      ManualCycleCompletionHandled = false;
      publishCycleValue();
      if (CycleValue > 0)
      {
        StartPunch();
      }
      else
      {
        ManualCycleActive = false;
        resetCycleButtonDisplay();
      }
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
    Interval = 60.0f * PhaseIntervalHours[Phase];
    saveTimerState();
  }
}

void updatePeriodicDisplay()
{
  if (AutoCycleEnabled == 1)
  {
    Interval -= 0.01666667;
    if (Interval <= 0.02)
    {
      s_PunchReason = "Time";
      Interval = 60.0f * PhaseIntervalHours[Phase];
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
