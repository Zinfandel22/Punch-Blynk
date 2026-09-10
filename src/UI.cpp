#include "UI.h"
#include "Actuators.h"
#include <Preferences.h>
#include <time.h>

// --- 1. GLOBAL HARDWARE OBJECTS ---
TFT_eSPI tft;

// --- 2. GLOBAL BUTTON OBJECTS ---
UiButton ButtonState;
UiButton ButtonPunch;
UiButton ButtonInfo;
UiButton ProfileButtons[4];
static const char *const ProfileNames[] = {"Innoc", "Active", "Lag", "Finish"};
static const int16_t ProfileButtonX[] = {60, 180, 300, 420};
static constexpr int16_t ProfileButtonY = 290;
static constexpr uint16_t ProfileButtonWidth = 110;
static constexpr uint16_t ProfileButtonHeight = 40;

// --- 3. GLOBAL TOUCH VARIABLES ---
int px, py, pz;
unsigned long lastTouchTime = 0;
bool touchActive = false;
unsigned long touchDelay = 250;
static Preferences touchPreferences;
static int cycleButtonDisplayedCount = 0;
static unsigned long cycleButtonDisplayUntil = 0;

void updateCurrentTime()
{
  char timeText[12] = "--:--:--";
  const time_t now = time(nullptr);
  if (now > 100000)
  {
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);
    strftime(timeText, sizeof(timeText), "%H:%M:%S", &timeInfo);
  }

  tft.fillRect(360, 4, 120, 31, BLACK);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setCursor(365, 14);
  tft.print(timeText);
}

void updateBinName()
{
  tft.fillRect(115, 4, 130, 31, BLACK);
  tft.setTextColor(CYAN, BLACK);
  tft.setTextSize(3);
  tft.setCursor(135, 10);
  tft.print(BinName);
}

static bool loadTouchCalibration(uint16_t calibrationData[5])
{
  touchPreferences.begin("touch", false);
  const bool calibrationDataValid = touchPreferences.isKey("calibration_v2") &&
                                    touchPreferences.getBytesLength("calibration_v2") == sizeof(uint16_t[5]) &&
                                    touchPreferences.getBytes("calibration_v2", calibrationData, sizeof(uint16_t[5])) == sizeof(uint16_t[5]);
  touchPreferences.end();
  return calibrationDataValid;
}

static void calibrateTouch()
{
  uint16_t calibrationData[5];
  Serial.println("Touch calibration required");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 20);
  tft.println("Touch the calibration points");
  tft.calibrateTouch(calibrationData, TFT_MAGENTA, TFT_BLACK, 15);
  touchPreferences.begin("touch", false);
  touchPreferences.putBytes("calibration_v2", calibrationData, sizeof(calibrationData));
  touchPreferences.end();
  Serial.println("Touch calibration saved");
  tft.setTouch(calibrationData);
}

void resetTouchCalibration()
{
  touchPreferences.begin("touch", false);
  touchPreferences.remove("calibration_v2");
  touchPreferences.end();
  calibrateTouch();
}

void UiButton::initButton(TFT_eSPI *displayInstance, int16_t x, int16_t y,
                          uint16_t buttonWidth, uint16_t buttonHeight,
                          uint16_t buttonOutline, uint16_t buttonFill,
                          uint16_t buttonTextColor, char *buttonLabel,
                          uint8_t buttonTextSize)
{
  display = displayInstance;
  centerX = x;
  centerY = y;
  width = buttonWidth;
  height = buttonHeight;
  outline = buttonOutline;
  fill = buttonFill;
  textColor = buttonTextColor;
  label = buttonLabel;
  textSize = buttonTextSize;
}

void UiButton::drawButton(bool inverted)
{
  if (display == nullptr)
    return;

  const uint16_t borderColor = inverted ? fill : outline;
  const uint16_t backgroundColor = inverted ? outline : fill;
  const uint16_t foregroundColor = inverted ? outline : textColor;
  const int16_t left = centerX - width / 2;
  const int16_t top = centerY - height / 2;

  display->fillRoundRect(left, top, width, height, 4, backgroundColor);
  display->drawRoundRect(left, top, width, height, 4, borderColor);
  display->setTextColor(foregroundColor, backgroundColor);
  display->setTextSize(textSize);

  int16_t textX = left + 4;
  int16_t textY = centerY - (8 * textSize) / 2;
  if (label != nullptr)
  {
    const uint16_t textWidth = display->textWidth(label);
    const uint16_t textHeight = display->fontHeight();
    textX = centerX - textWidth / 2;
    textY = centerY - textHeight / 2;
  }
  display->setCursor(textX, textY);
  display->print(label == nullptr ? "" : label);
}

bool UiButton::contains(int16_t x, int16_t y) const
{
  return x >= centerX - width / 2 && x <= centerX + width / 2 &&
         y >= centerY - height / 2 && y <= centerY + height / 2;
}

struct TouchRect
{
  int left;
  int top;
  int right;
  int bottom;
};

static bool pointInRect(int x, int y, const TouchRect &rect)
{
  return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

static void drawUiButton(UiButton &button, int x, int y,
                        uint16_t outline, uint16_t fill, uint16_t textColor,
                        const char *label, uint8_t textSize)
{
  button.initButton(&tft, x, y, BUTTON_W, BUTTON_H, outline, fill, textColor, (char *)label, textSize);
  button.drawButton(false);
}

static void drawSizedUiButton(UiButton &button, int x, int y, uint16_t width,
                              uint16_t height, uint16_t outline, uint16_t fill,
                              uint16_t textColor, const char *label, uint8_t textSize)
{
  button.initButton(&tft, x, y, width, height, outline, fill, textColor, (char *)label, textSize);
  button.drawButton(false);
}

static void printCentered(const String &value, int16_t centerX, int16_t y)
{
  tft.setCursor(centerX - tft.textWidth(value) / 2, y);
  tft.print(value);
}

static void renderStateButton(UiButton &button, int x, int y,
                              bool active, const char *activeLabel,
                              const char *inactiveLabel, uint16_t activeFill,
                              uint16_t activeTextColor, uint16_t inactiveFill,
                              uint16_t inactiveTextColor)
{
  drawUiButton(button, x, y, WHITE,
               active ? activeFill : inactiveFill,
               active ? activeTextColor : inactiveTextColor,
               active ? activeLabel : inactiveLabel, 2);
}

static void renderSizedStateButton(UiButton &button, int x, int y, uint16_t width,
                                   uint16_t height, bool active,
                                   const char *activeLabel, const char *inactiveLabel,
                                   uint16_t activeFill, uint16_t activeTextColor,
                                   uint16_t inactiveFill, uint16_t inactiveTextColor)
{
  drawSizedUiButton(button, x, y, width, height, WHITE,
                    active ? activeFill : inactiveFill,
                    active ? activeTextColor : inactiveTextColor,
                    active ? activeLabel : inactiveLabel, 2);
}

static void drawProfileButtons()
{
  for (byte profile = 0; profile < 4; profile++)
  {
    ProfileButtons[profile].initButton(&tft, ProfileButtonX[profile], ProfileButtonY,
                                       ProfileButtonWidth, ProfileButtonHeight,
                                       WHITE, profile == Phase ? GREEN : BLACK,
                                       profile == Phase ? BLACK : WHITE,
                                       (char *)ProfileNames[profile], 2);
    ProfileButtons[profile].drawButton(false);
  }
}

void initDisplay()
{
  Serial.println("Calling TFT init");
  tft.init();
  Serial.println("TFT init complete");
  tft.setRotation(1);
  Serial.print("TFT dimensions after rotation: ");
  Serial.print(tft.width());
  Serial.print(" x ");
  Serial.println(tft.height());
  uint16_t calibrationData[5];
  if (loadTouchCalibration(calibrationData))
  {
    tft.setTouch(calibrationData);
    Serial.println("Touch calibration loaded");
  }
  else
  {
    calibrateTouch();
  }
  Serial.println("Touch setup complete");

    tft.fillScreen(BLACK);
    tft.setTextSize(3);
    tft.setTextColor(RED);
    tft.setCursor(15, 10);
    tft.println("Satori Cellars");
    tft.drawLine(10, 42, 470, 42, GREEN);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(15, 50);
    tft.println(Version);
    tft.setCursor(15, 80);
    tft.setTextColor(CYAN);
    tft.println(Concept0);
    tft.setTextColor(WHITE);
    tft.setCursor(15, 105);
    tft.println(Concept1);
    tft.setCursor(15, 125);
    tft.println(Concept2);
    delay(2000);
}

void drawMainScreen()
{ // Main operating screen
  touchActive = false;
  tft.fillScreen(BLACK); // Clear screen
  Serial.print("Time since last punch = ");
  Serial.print((millis() - lastPunchCompleteTime) / 1000);
  Serial.println(" s");
  // TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(15, 10);
  tft.println("Satori");
  updateCurrentTime();
  updateBinName();
  tft.drawLine(10, 42, 480, 42, GREEN);

  // Labels
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(15, 50);
  tft.println("Timer");
  tft.setCursor(145, 50);
  tft.println("Interval");
  tft.setCursor(285, 50);
  tft.println("Last");
  tft.setCursor(15, 68);
  tft.println("(hms)");
  tft.setCursor(145, 68);
  tft.println("(h)");
  tft.setCursor(285, 68);
  tft.println("(m)");
  tft.setCursor(15, 165);
  tft.println("Temp");
  tft.setCursor(145, 165);
  tft.println("Set Temp");
  tft.setCursor(285, 165);
  tft.println("Max Temp");
  tft.setCursor(15, 183);
  tft.println("(F)");
  tft.setCursor(145, 183);
  tft.println("(F)");
  tft.setCursor(285, 183);
  tft.println("(F)");

  // Data
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();
  tft.fillRect(135, 208, 110, 40, BLACK);
  printCentered(String(TempSetPoint[Phase]), 190, 213);
  tft.fillRect(285, 208, 70, 40, BLACK);
  tft.setCursor(295, 213);
  tft.println(TMax, 1);
  updateInterval();
  
  // BUTTONS
  renderSizedStateButton(ButtonState, 420, 70, 110, 40, AutoCycleEnabled,
                    "Running", "Stopped", BLACK, RED, BLACK, GREEN);

  drawSizedUiButton(ButtonPunch, 420, 140, 110, 40, WHITE, BLUE, WHITE, "Cycle", 2);
  drawSizedUiButton(ButtonInfo, 420, 210, 110, 40, WHITE, PURPLE, WHITE, "Shake", 2);
  drawProfileButtons();
  updateActuatorState();
  updateShakeButton();
  updateCycleButton();
}
/*END----------------------------------------------------------------------------------------------*/

void ReadScreen()
{
  if (!ScreenTouched())
    return;

  handleTempAdjust();
  handleRunStop();
  handleManualCycle();
  handleInfoPage();
  handleProfileButton();
}

/*END----------------------------------------------------------------------------------------------*/

void handleTempAdjust()
{
  if (pointInRect(px, py, {60, 120, 150, 240}))
  {
    if (py <= 200 && TempSetPoint[Phase] < 120)
    {
      TempSetPoint[Phase]++;
    }
    else if (py > 200 && TempSetPoint[Phase] > 50)
    {
      TempSetPoint[Phase]--;
    }
    TempSetPoint[Phase] = constrain(TempSetPoint[Phase], 50, 120);
    preferences.putBytes("tempSetPoint", TempSetPoint, 4 * sizeof(TempSetPoint[0]));
    tft.fillRect(135, 208, 110, 40, BLACK);
    tft.setTextSize(3);
    printCentered(String(TempSetPoint[Phase]), 190, 213);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleRunStop()
{
  if (ButtonState.contains(px, py))
  {
    AutoCycleEnabled = !AutoCycleEnabled;
    preferences.putBool("autoCycle", AutoCycleEnabled);
    if (AutoCycleEnabled == 0)
    {
      AbortPunch();
    }
    renderSizedStateButton(ButtonState, 420, 70, 110, 40, AutoCycleEnabled,
                      "Running", "Stopped", BLACK, RED, BLACK, GREEN);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleManualCycle()
{
  if (ButtonPunch.contains(px, py))
  {
    if (CycleValue >= 5)
      return;

    CycleValue++;
    showCycleButtonCount(CycleValue);
    if (!PunchActive)
    {
      ManualCycleActive = true;
      s_PunchReason = "Manual";
      TempDwellTime = 0;
      preferences.putInt("runDwellTime", TempDwellTime);
      ManualCycleCompletionHandled = false;
      StartPunch();
    }
    publishBlynkState();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleInfoPage()
{
  if (ButtonInfo.contains(px, py))
  {
    if (!PunchActive)
    {
      StartShake();
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleProfileButton()
{
  for (byte profile = 0; profile < 4; profile++)
  {
    if (!ProfileButtons[profile].contains(px, py))
      continue;

    Phase = profile;
    preferences.putUChar("phase", Phase);
    applyPhaseSettingsPreservingTimer();
    drawProfileButtons();
    publishBlynkState();
    Serial.print("Profile selected: ");
    Serial.println(ProfileNames[Phase]);
    return;
  }
}
/*END----------------------------------------------------------------------------------------------*/

void updateTemp()
{ // Display update of Temperature
  GetTemp();
  tft.fillRect(15, 208, 80, 40, BLACK);
  tft.setTextSize(3);
  tft.setTextColor(WHITE);
  tft.setCursor(15, 213);
  tft.println(TempAct, 1);
  tft.fillRect(285, 208, 70, 40, BLACK);
  tft.setCursor(295, 213);
  tft.println(TMax, 1);
}
/*END----------------------------------------------------------------------------------------------*/

void updateActuatorState()
{
  static char lastStateLabel[5] = "";
  const String currentStateLabel = actuatorStateLabel();
  const char *stateLabel = currentStateLabel.c_str();

  if (strcmp(lastStateLabel, stateLabel) == 0)
    return;

  tft.fillRect(250, 0, 100, 40, BLACK);
  strcpy(lastStateLabel, stateLabel);

  if (stateLabel[0] == '\0')
    return;

  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(255, 8);
  tft.print(stateLabel);
}

String actuatorStateLabel()
{
  const PunchState state = actuators.getState();
  if (state != PUNCH_DOWN && state != PUNCH_UP)
    return String();

  char stateLabel[5];
  const char direction = state == PUNCH_DOWN ? 'v' : '^';
  snprintf(stateLabel, sizeof(stateLabel), "%d%c",
           actuators.getCurrentActuator() + 1, direction);
  return String(stateLabel);
}

String actuatorCloudStateLabel()
{
  const PunchState state = actuators.getState();
  if (state != PUNCH_DOWN && state != PUNCH_UP)
    return String();

  return String(actuators.getCurrentActuator() + 1) +
         (state == PUNCH_DOWN ? " descending" : " ascending");
}

void updateShakeButton()
{
  static bool lastShakeState = false;
  const PunchState state = actuators.getState();
  const bool shaking = state == PUNCH_SHAKE_WAIT ||
                       state == PUNCH_SHAKE_DOWN ||
                       state == PUNCH_SHAKE_UP;

  if (shaking == lastShakeState)
    return;

  lastShakeState = shaking;
  drawSizedUiButton(ButtonInfo, 420, 210, 110, 40, WHITE, PURPLE, WHITE,
                    shaking ? "Shaking" : "Shake", 2);
}

void showCycleButtonCount(int count)
{
  cycleButtonDisplayedCount = count;
  cycleButtonDisplayUntil = millis() + 1000;
  drawSizedUiButton(ButtonPunch, 420, 140, 110, 40, WHITE, BLUE, WHITE,
                    String(cycleButtonDisplayedCount).c_str(), 2);
}

void updateCycleButton()
{
  static String lastLabel;
  String label;

  if (cycleButtonDisplayUntil != 0 && millis() < cycleButtonDisplayUntil)
  {
    label = String(cycleButtonDisplayedCount);
  }
  else
  {
    cycleButtonDisplayUntil = 0;
    label = CycleValue > 0 ? "Cycling" : "Cycle";
  }

  if (label == lastLabel)
    return;

  lastLabel = label;
  drawSizedUiButton(ButtonPunch, 420, 140, 110, 40, WHITE, BLUE, WHITE,
                    label.c_str(), 2);
}

void resetCycleButtonDisplay()
{
  cycleButtonDisplayUntil = 0;
  updateCycleButton();
}

void updateInterval()
{                              // Display Interval till update
  int hours = (Interval / 60); // Hours
  int minutes = ((Interval / 60) - hours) * 60;
  int seconds = (((Interval / 60) - hours) * 60 - minutes) * 60;
  String s_NextTime;
  String _hour = String(hours);
  String _min = String(minutes);
  String _sec = String(seconds);
  if (_min.length() == 1)
  {
    _min = "0" + _min;
  }
  if (_sec.length() == 1)
  {
    _sec = "0" + _sec;
  }
  s_NextTime = _hour + ":" + _min + ":" + _sec;
  tft.setTextSize(3);
  tft.fillRect(15, 95, 130, 35, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(15, 100);
  tft.println(s_NextTime); // Time to next Punch
  tft.fillRect(295, 95, 60, 35, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(295, 100);
  tft.println(TempDwellTime);
  tft.fillRect(145, 95, 100, 35, BLACK);
  printCentered(String(PhaseIntervalHours[Phase]), 190, 100);
}
/*END----------------------------------------------------------------------------------------------*/

bool ScreenTouched()
{
  static bool lastReportedTouchState = false;
  uint16_t touchX = 0;
  uint16_t touchY = 0;
  const bool isTouched = tft.getTouch(&touchX, &touchY, 100);
  px = touchX;
  py = touchY;
  pz = isTouched ? 1 : 0;

  if (isTouched && !lastReportedTouchState)
  {
    Serial.print("Touch detected: mapped x=");
    Serial.print(px);
    Serial.print(", mapped y=");
    Serial.println(py);
  }
  else if (!isTouched && lastReportedTouchState)
  {
    Serial.println("Touch released");
  }
  lastReportedTouchState = isTouched;

  unsigned long now = millis();

  // If still touching, hold state but don't repeat
  if (isTouched && !touchActive && (now - lastTouchTime >= touchDelay))
  {
    touchActive = true;
    lastTouchTime = now;
    Serial.print("Touch accepted: x=");
    Serial.print(px);
    Serial.print(", y=");
    Serial.println(py);
    Serial.print(" state=");
    Serial.print(ButtonState.contains(px, py));
    Serial.print(" punch=");
    Serial.print(ButtonPunch.contains(px, py));
    Serial.print(" info=");
    Serial.print(ButtonInfo.contains(px, py));
    Serial.println();
    return true; // New touch event registered
  }

  // When released, reset touch state so next press can register
  if (!isTouched)
  {     touchActive = false;
  }
  return false; // No new touch detected
}
/*END----------------------------------------------------------------------------------------------*/