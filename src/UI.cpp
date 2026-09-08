#include "UI.h"
#include "Actuators.h"

// --- 1. GLOBAL HARDWARE OBJECTS ---
MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// --- 2. GLOBAL BUTTON OBJECTS ---
Adafruit_GFX_Button ButtonState;
Adafruit_GFX_Button ButtonPunch;
Adafruit_GFX_Button ButtonInfo;
Adafruit_GFX_Button ButtonReturn;
Adafruit_GFX_Button ButtonStart;
Adafruit_GFX_Button ButtonReset;
Adafruit_GFX_Button ButtonConfig;
Adafruit_GFX_Button ButtonCycle;
Adafruit_GFX_Button ButtonCount;
Adafruit_GFX_Button ButtonShake;

// --- 3. GLOBAL TOUCH VARIABLES ---
int px, py, pz;
unsigned long lastTouchTime = 0;
bool touchActive = false;
unsigned long touchDelay = 250;
static float lastStatusTMin = -9999.0f;
static float lastStatusTMax = -9999.0f;

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

static bool isPageButtonHit(byte page, Adafruit_GFX_Button &button, int x, int y)
{
  return CurrentPage == page && button.contains(x, y);
}

static bool isPageTouchRect(byte page, int x, int y, const TouchRect &rect)
{
  return CurrentPage == page && pointInRect(x, y, rect);
}

static void drawUiButton(Adafruit_GFX_Button &button, int x, int y,
                        uint16_t outline, uint16_t fill, uint16_t textColor,
                        const char *label, uint8_t textSize)
{
  button.initButton(&tft, x, y, BUTTON_W, BUTTON_H, outline, fill, textColor, (char *)label, textSize);
  button.drawButton(true);
}

static void renderStateButton(Adafruit_GFX_Button &button, int x, int y,
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

void initDisplay()
{
    uint16_t identifier = tft.readID();
    if (identifier == 0x0101 || identifier == 0x0000 || identifier == 0xFFFF)
    {
        identifier = 0x9341;
    }
    tft.begin(identifier);
    tft.setRotation(1);

    tft.fillScreen(BLACK);
    tft.setTextSize(3);
    tft.setTextColor(RED);
    tft.setCursor(15, 10);
    tft.println("Satori Cellars");
    tft.drawLine(10, 42, 320, 42, GREEN);
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
  tft.println("Satori Cellars");
  tft.drawLine(10, 42, 320, 42, GREEN);

  // Labels
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(15, 50);
  tft.println("Temp(F)");
  tft.setCursor(15, 125);
  tft.println("Set");
  tft.setCursor(15, 140);
  tft.println("Temp(F)");
  tft.setCursor(125, 50);
  tft.println("Time(hms)");
  tft.setCursor(125, 140);
  tft.println("Mix/day");
  tft.setCursor(125, 98);
  tft.setTextColor(CYAN);
  tft.println("Temp(m)"); // Time to next Punch

  // Incrementors
  tft.setTextSize(3);
  tft.setTextColor(GREEN); // UP
  tft.setCursor(80, 160);  // Temp UP
  tft.println("+");
  tft.setCursor(190, 160); // Interval UP
  tft.println("+");

  tft.setTextColor(RED);  // DOWN
  tft.setCursor(80, 200); // Temp DOWN
  tft.println("-");
  tft.setCursor(190, 200); // Interval Down
  tft.println("-");

  // Data
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();
  tft.setCursor(20, 175);
  tft.println(TempSetPoint);
  updateInterval();
  UpdateSetInterval();
  tft.setTextSize(2);
  tft.setTextColor(GREENYELLOW);
  tft.setCursor(0, 222);
  tft.println("Last Punch Reason: ");
  tft.setTextColor(RED);
  tft.setCursor(245, 222);
  tft.println(s_PunchReason);

  // BUTTONS
  renderStateButton(ButtonState, 280, 75, AutoCycleEnabled,
                    "Pause", "Run", WHITE, RED, BLACK, GREEN);

  drawUiButton(ButtonPunch, 280, 135, WHITE, WHITE, BLUE, "Cycle", 2);
  drawUiButton(ButtonInfo, 280, 195, WHITE, WHITE, BLUE, "Status", 2);
  updateActuatorState();
}
/*END----------------------------------------------------------------------------------------------*/

void drawManualScreen()
{ // Manual Punch Screen
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  tft.setCursor(5, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Cycle Punches");
  // DIVIDER LINES
  tft.drawLine(10, 42, 320, 42, GREEN); // Title

  // tft.drawLine(215, 42, 215, 230, GREEN);  //Vertical Center

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(15, 50);
  tft.println("Temp(F)");
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();

  renderStateButton(ButtonCycle, 280, 75, ManualCycleActive,
                    "Stop", "Run", WHITE, RED, BLACK, GREEN);

  static char labelBuffer[5];
  sprintf(labelBuffer, "%d", CycleValue);

  drawUiButton(ButtonCount, 280, 135, WHITE, WHITE, BLUE, labelBuffer, 2);
  drawUiButton(ButtonShake, 280, 195, WHITE, BLACK, PURPLE, "Shake", 2);
}
/*END----------------------------------------------------------------------------------------------*/

void drawInfoScreen()
{ // Running status screen
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  // TITLE
  tft.setCursor(15, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Status");
  // DIVIDERS LINES
  tft.drawLine(10, 42, 320, 42, GREEN); // Title
  tft.drawLine(115, 45, 115, 240, GREEN);

  // TEMP
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  // TITLES
  tft.setCursor(5, 50);
  tft.println("Temp (F)");

  tft.setCursor(5, 110);
  tft.println("Temp Min");

  tft.setCursor(5, 180);
  tft.println("Temp Max");

  tft.setCursor(125, 50);
  tft.println("Cycles");

  tft.setCursor(125, 110);
  tft.println("Uptime");

  tft.setCursor(125, 180);
  tft.println("Start Delay (m)");

  // VALUES
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 75);
  tft.println(TempAct, 1);

  tft.setCursor(20, 130);
  tft.println(TMin, 1);

  tft.setCursor(20, 200);
  tft.println(TMax, 1);

  tft.setCursor(125, 70);
  tft.println(Cycles);

  Uptime();
  tft.setCursor(125, 130);
  tft.println(s_UpTime);

  Uptime();
  tft.setCursor(125, 130);
  tft.println(s_UpTime);

  tft.setCursor(170, 200);
  tft.println(StartOffset[_days]);

  // Reduce Interval Down
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(120, 200);
  tft.println("-");

  // Increase Interval Up
  tft.setTextColor(GREEN);
  tft.setCursor(250, 200);
  tft.println("+");

  // BUTTONS
  drawUiButton(ButtonReset, 280, 75, WHITE, WHITE, RED, "Reset", 2);
  drawUiButton(ButtonConfig, 280, 135, WHITE, WHITE, BLUE, "Config", 2);
  InfoAge = millis();
}
/*END----------------------------------------------------------------------------------------------*/

void drawConfigScreen()
{ // Persistent config settings
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  // TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(10, 10);
  tft.println("Plunge Config");
  // DIVIDERS LINES
  tft.drawLine(0, 42, 320, 42, GREEN); // Title

  // Increment selectors
  //  UP
  tft.setTextColor(GREEN);
  tft.setCursor(70, 80); // Speed Down - Increase
  tft.println("+");
  tft.setCursor(230, 80); // Time Cycles Increase
  tft.println("+");
  tft.setCursor(70, 177); // Temp Dwell Increase
  tft.println("+");
  tft.setCursor(230, 177); // Temp Cycles Increase
  tft.println("+");

  // DOWN
  tft.setTextColor(RED);
  tft.setCursor(70, 115); // Speed Down - Decrease
  tft.println("-");
  tft.setCursor(230, 115); // Time Cycles Decrease
  tft.println("-");
  tft.setCursor(70, 207); // Temp Dwell Decrease
  tft.println("-");
  tft.setCursor(230, 207); // Temp Cycles Decrease
  tft.println("-");

  // Labels
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(10, 50);
  tft.println("Plunge Time");
  tft.setCursor(10, 66);
  tft.println("Down (s)");
  tft.setCursor(10, 150);
  tft.println("Temp Delay");
  tft.setCursor(10, 166);
  tft.println("(min)");
  tft.setCursor(185, 50);
  tft.println("Cycles/Time");
  tft.setCursor(185, 150);
  tft.println("Cycles/Temp");

  // VALUES
  tft.setTextColor(WHITE);
  tft.setTextSize(3);

  tft.setCursor(15, 95);
  tft.println(StrokeDownTime);
  tft.setCursor(15, 190);
  tft.println(SetTempDwellTime);
  tft.setCursor(180, 95);
  tft.println(SetTimeRep_UI);
  tft.setCursor(180, 190);
  tft.println(SetTempRep_UI);

  InfoAge = millis(); // Reset screen timeout counter
}
/*END----------------------------------------------------------------------------------------------*/

void ReadScreen()
{
  if (!ScreenTouched())
    return;
  InfoAge = millis();

  handleTempAdjust();
  handleIntervalAdjust();
  handleRunStop();
  handleManualCycle();
  handleInfoPage();
  handleManualCycleActions();
  handleStatusReset();
  handleOffsetAdjust();
  handleConfigAdjustments();
  handleHomeIcon();
}

/*END----------------------------------------------------------------------------------------------*/

/* Screen Handlers*/
void handleTempAdjust()
{
  if (isPageTouchRect(1, px, py, {60, 120, 150, 240}))
  {
    if (py <= 200 && TempSetPoint < 120)
    {
      TempSetPoint++;
    }
    else if (py > 200 && TempSetPoint > 50)
    {
      TempSetPoint--;
    }
    EEPROM.update(4, TempSetPoint);
    tft.fillRect(20, 170, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(20, 175);
    tft.println(TempSetPoint);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleIntervalAdjust()
{
  if (isPageTouchRect(1, px, py, {150, 120, 220, 240}))
  {
    if (py <= 200 && Index < 7)
    {
      Index++;
    }
    else if (py > 200 && Index > 0)
    {
      Index--;
    }
    EEPROM.update(1, Index);
    IntervalSet = _IntervalSet[Index];
    Interval = 1440 / IntervalSet + _days * 5;
    UpdateSetInterval();
    updateInterval();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleRunStop()
{
  if (isPageButtonHit(1, ButtonState, px, py))
  {
    AutoCycleEnabled = !AutoCycleEnabled;
    EEPROM.update(0, AutoCycleEnabled);
    if (AutoCycleEnabled == 0)
    {
      AbortPunch();
    }
    renderStateButton(ButtonState, 280, 75, AutoCycleEnabled,
                      "Pause", "Run", WHITE, RED, BLACK, GREEN);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleManualCycle()
{
  Adafruit_GFX_Button *button = nullptr;
  if (CurrentPage == 1)
    button = &ButtonPunch;
  else if (CurrentPage == 2)
    button = &ButtonConfig;
  else if (CurrentPage == 3)
    button = &ButtonCount;

  if (button != nullptr && button->contains(px, py))
  {
    switch (CurrentPage)
    {
    case 1:
      CurrentPage = 3;
      drawManualScreen();
      break;
    case 2:
      CurrentPage = 4;
      drawConfigScreen();
      break;
    case 3:
      CycleIndex = (CycleIndex + 1) % 4;
      CycleValue = cycleValues[CycleIndex];
      static char labelBuffer[5];
      sprintf(labelBuffer, "%d", CycleValue);
      drawUiButton(ButtonCount, 280, 135, WHITE, WHITE, BLUE, labelBuffer, 2);
      break;
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleInfoPage()
{
  if (isPageButtonHit(1, ButtonInfo, px, py))
  {
    CurrentPage = 2;
    drawInfoScreen();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleManualCycleActions()
{
  if (CurrentPage == 3)
  {
    if (ButtonCycle.contains(px, py))
    {
      if (!ManualCycleActive)
      {
        if (PunchActive)
          return;

        ManualCycleActive = true;
        renderStateButton(ButtonCycle, 280, 75, true,
                          "Stop", "Run", WHITE, RED, BLACK, GREEN);
        s_PunchReason = "Manual";
        SetPunchReps = CycleValue;
        completedCycles = 1;
        ManualCycleCompletionHandled = false;
        StartPunch();
      }
      else
      {
        ManualCycleActive = false;
        AbortPunch();
        renderStateButton(ButtonCycle, 280, 75, false,
                          "Stop", "Run", WHITE, RED, BLACK, GREEN);
      }
    }
    if (ButtonShake.contains(px, py) && !PunchActive)
    {
      StartShake();
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleStatusReset()
{
  if (isPageButtonHit(2, ButtonReset, px, py))
  {
    TMax = 0;
    TMin = 99;
    Cycles = 0;
    EEPROM.update(10, Cycles);
    EEPROM.update(5, 1);
    AbortPunch();
    drawInfoScreen();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleOffsetAdjust()
{
  if (isPageTouchRect(2, px, py, {100, 150, 270, 240}))
  {
    if (px < 180)
    {
      _days = (_days == 0) ? 12 : _days - 1;
    }
    else
    {
      _days = (_days == 12) ? 0 : _days + 1;
    }
    EEPROM.update(11, _days);
    tft.fillRect(170, 197, 45, 30, BLACK);
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(170, 200);
    tft.println(StartOffset[_days]);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleConfigAdjustments()
{
  if (CurrentPage != 4)
    return;

  // Stroke Down Time
  if (pointInRect(px, py, {20, 30, 180, 160}))
  {
    if (py < 100 && StrokeDownTime < 90)
      StrokeDownTime += 5;
    else if (py >= 100 && StrokeDownTime > 5)
      StrokeDownTime -= 5;
    EEPROM.update(8, StrokeDownTime);
    tft.fillRect(10, 90, 40, 35, BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 95);
    tft.println(StrokeDownTime);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Time Reps
  if (pointInRect(px, py, {180, 30, 280, 160}))
  {
    if (py < 100 && SetTimeRep_UI < 10)
      SetTimeRep_UI++;
    else if (py >= 100 && SetTimeRep_UI > 1)
      SetTimeRep_UI--;
    EEPROM.update(6, SetTimeRep_UI);
    tft.fillRect(170, 90, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(180, 95);
    tft.println(SetTimeRep_UI);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Temp Reps
  if (pointInRect(px, py, {180, 170, 280, 235}))
  {
    if (py < 200 && SetTempRep_UI < 10)
      SetTempRep_UI++;
    else if (py >= 200 && SetTempRep_UI > 1)
      SetTempRep_UI--;
    EEPROM.update(7, SetTempRep_UI);
    tft.fillRect(170, 180, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(180, 190);
    tft.println(SetTempRep_UI);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Temp Dwell
  if (pointInRect(px, py, {20, 170, 180, 235}))
  {
    if (py < 200 && SetTempDwellTime < 240)
      SetTempDwellTime += 5;
    else if (py >= 200 && SetTempDwellTime > 0)
      SetTempDwellTime -= 5;
    EEPROM.update(9, SetTempDwellTime);
    tft.fillRect(15, 190, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 190);
    tft.println(SetTempDwellTime);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleHomeIcon()
{
  if (CurrentPage != 1 && ButtonReturn.contains(px, py))
  {
    CurrentPage = 1;
    drawMainScreen();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void updateTemp()
{ // Display update of Temperature
  if (CurrentPage != 4)
  {
    GetTemp();
    tft.fillRect(20, 65, 70, 40, BLACK);
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(20, 75);
    tft.println(TempAct, 1);
  }
  if (CurrentPage == 3)
  {
    if (ManualCycleActive)
    {
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 140);
      tft.print("Cycles ");
      tft.fillRect(90, 140, 20, 20, BLACK);
      tft.print(completedCycles);
      tft.print(" of ");
      tft.fillRect(160, 140, 20, 20, BLACK);
      tft.print(SetPunchReps);
    }
    else
    {
      tft.fillRect(15, 138, 180, 20, BLACK);
    }
  }
  if (CurrentPage == 2)
  {
    tft.setTextSize(3);
    tft.setTextColor(WHITE);

    if (TMin != lastStatusTMin)
    {
      lastStatusTMin = TMin;
      tft.fillRect(20, 125, 70, 35, BLACK);
      tft.setCursor(20, 130);
      tft.println(TMin, 1);
    }

    if (TMax != lastStatusTMax)
    {
      lastStatusTMax = TMax;
      tft.fillRect(20, 195, 70, 35, BLACK);
      tft.setCursor(20, 200);
      tft.println(TMax, 1);
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void updateActuatorState()
{
  if (CurrentPage != 1)
    return;

  tft.fillRect(275, 0, 45, 40, BLACK);

  const PunchState state = actuators.getState();
  if (state != PUNCH_DOWN && state != PUNCH_UP)
    return;

  char stateLabel[5];
  const char direction = state == PUNCH_DOWN ? 'v' : '^';
  snprintf(stateLabel, sizeof(stateLabel), "%d%c",
           actuators.getCurrentActuator() + 1, direction);

  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(280, 8);
  tft.print(stateLabel);
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
  tft.setTextSize(2);
  tft.fillRect(125, 68, 100, 25, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 75);
  tft.println(s_NextTime); // Time to next Punch
  tft.setTextSize(2);
  tft.fillRect(125, 115, 60, 20, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 117);
  tft.println(TempDwellTime); // Time to next Punch
}
/*END----------------------------------------------------------------------------------------------*/

void UpdateSetInterval()
{
  tft.setTextSize(3);
  tft.setCursor(125, 175);
  tft.fillRect(110, 170, 60, 40, BLACK); // Clear per day
  tft.println(_IntervalSet[Index]);
  updateInterval();
}
/*END----------------------------------------------------------------------------------------------*/

void drawhomeicon()
{ // draws a white home icon
  ButtonReturn.initButton(&tft, 300, 20, 40, 40, BLACK, BLACK, WHITE, (char *)"", 1);
  tft.drawLine(280, 19, 299, 0, WHITE);
  tft.drawLine(300, 0, 304, 4, WHITE);
  tft.drawLine(304, 3, 304, 0, WHITE);
  tft.drawLine(305, 0, 307, 0, WHITE);
  tft.drawLine(308, 0, 308, 8, WHITE);
  tft.drawLine(309, 9, 319, 19, WHITE);
  tft.drawLine(281, 19, 283, 19, WHITE);
  tft.drawLine(316, 19, 318, 19, WHITE);
  tft.drawRect(284, 19, 32, 21, WHITE);
  tft.drawRect(295, 25, 10, 15, WHITE);
}
/*END----------------------------------------------------------------------------------------------*/

bool ScreenTouched()
{
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  // Get x,y&z values and transpose (There is a better way than this but this works)
  p.x = (tft.height() - map(p.x, TS_LEFT, TS_RIGHT, tft.height(), 0)); // Scale range to adjust to rotation from Portrait to landscape
  p.y = (tft.width() - map(p.y, TS_BOT, TS_TOP, tft.width(), 0));
  px = p.y;
  py = p.x;
  pz = p.z;
  bool isTouched = (pz != 0 && ((pz > MINPRESSURE && pz < MAXPRESSURE) || (px > 100 && px < 900 && py > 100 && py < 900)));

  unsigned long now = millis();

  // If still touching, hold state but don't repeat
  if (isTouched && !touchActive && (now - lastTouchTime >= touchDelay))
  {
    touchActive = true;
    lastTouchTime = now;
    Serial.print("x:");
    Serial.print(px);
    Serial.print(", y:");
    Serial.print(py);
    Serial.print(", z:");
    Serial.println(pz);
    return true; // New touch event registered
  }

  // When released, reset touch state so next press can register
  if (!isTouched)
  {     touchActive = false;
  }
  return false; // No new touch detected
}
/*END----------------------------------------------------------------------------------------------*/