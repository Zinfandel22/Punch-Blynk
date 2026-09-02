/*INTRO
*-------------------------------------------------------------------------------------------------
* Objective is to automate the wine must cap management with a solution 
  that simulates the manual process as closely as possible while providing 
  time and temp-initiated events

*OWNERSHIP
 *-----------------------------------------------------
 * Created by: Dave Williams 
 * Contact Info: dave@satoricellars.com
 * Wine Must Punch Controller
 * Dec 2022
 *-----------------------------------------------------*

*CODE HISTORY
*-------------------------------------------------------------------------------------------------
* NEW December 2022
*- Set up Main and Config Page
*- Add Stroke Time
*- Add Plunge repeats
*- Add Temp repeats and delay
*- Add temp and time punch loops 
*- Added Interval rollback logic
*- '23 Jan 15 Added Cycle mode
*- '23 Jan 31 Fixed interval rolback logic after testing
*- '23 Jan 31 Rearranged routines for interval reduction after long term tests*
*- '23 Apr 15 Updated cycle routine to better show state on buttons
*- '23 Jul 26 Revised plung sequences to be all down or all up rather than down and up individually
*- '23 Aug 22 Revised Manual punch sequence amd added Version at startup to better track many units
*- '23 Oct 03 Changed punch sequence to reverse the up from the down
*- '23 Oct 09 Removed Plunger selection on manual screen, added temperature and only Cycle. Need to move cycle to Main
*- '23 Oct 10 Restored plunge sequence. Set temp step+/- to 1. Allowed Dwell to 90s down ownly
*- '23 Oct 11 Added shake to end of each up routine
*- '23 Oct 11B Fixed bug on temp plunge start due to last plunge not set.
*- '23 Oct 11c Fixed issue with reset shake on home 
*- '23 Oct 13 - Run/stop to run pause and no reset
*-            - Temp timer on main screen
*-            - Fixed issues with restart after powe on and non screen read during Wait and up stroke
*- '23 Oct 14 - Added LastPunch reason
*- '23 Oct 14 - removed shake if no punch
*- '23 Oct 23 - refreshed last punch reason, Added 7s Wait display on cycle up, Fixed Red font on time after punch reason Update
*- '23 Oct 25 Home stops running routine issue, Timeout and reset on cycle when not running
*- '23 Oct 25b Issue with Cycle entry screen age out counter not set
*- '23 Oct 25c Issue with upstroke age if cycle canclled early
*- '23 Oct 29 Reworked all plunge and wait routines and used a passed parameters for Wait duration and plunger id for display
*-            Updated Temp Min routune as part of get temp to adddress setting 0 as min from an error condition
*-            Moved all display actions to Wait routine
*-            Renamed all plungers a,b,c,d to separate from numeric wait periods
*-            Set up character arrays for display
*-            Fixed lack of wait due to resetflag not being reinitialized after set.
*- '23 Nov 06 Added up dwell config
*- '23 Nov 16 MAJOR update to plunge sequencing so one stays down while the next goes down and the previous one come up.
*_ '24 Oct 03 Update to the shake to wait after up to ensure it hits the top stop.
*_            Cycle Pause not being read.
*_ '25 Oct 13 Refactored code to remove blocking, changed punch and shake to a State driven machine
*_ '26 Sep 02 Migrated to platformIO and started refactoring to current drivers
*/
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>             // Required background bus for TFT screens
#include <Wire.h>            // Required background bus for I2C devices

// Include compiled libraries from local lib/ or PlatformIO registry
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>

// --- Forward Declarations (Required for standard C++ compilation compliance) ---
void ReadScreen();
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
void drawMainScreen();
void drawInfoScreen();
void drawManualScreen();
void drawConfigScreen();
void updateInterval();
void updateTemp();
void drawhomeicon();
float GetTemp();
void Cycle();
void UpdateSetInterval();
String Uptime();
bool ScreenTouched();
void StartPunch();
void AbortPunch();
void UpdatePunch();
void StartShake();
void UpdateShakeDelay();
void RunBlockingShake();
void AllActuatorsUp();

#define Version "(c)2023-26 PIO-26.09.02"  // Used on boot display
#define Concept0 "Concept & Design"
#define Concept1 "Tom Moller"
#define Concept2 "Dave Williams"



//DEFINE HW control pins
/*-------------------------------------------------------------------------------------------------*/
#define ONE_WIRE_BUS 23  // Serial data wire is on pin 22 for dallas temp sensor
#define Sol1 25          // Solenoid 1 for Act1
#define Sol2 27          // Solenoid 2 for Act2
#define Sol3 29          // Solenoid 3 for Act3
#define Sol4 31          // Solenoid 4 for Act4

//DEFINE OneWire INSTANCE TO COMMUNICATE WITH MAXIM/DALLAS TEMPERATURE SENSOR
/*-------------------------------------------------------------------------------------------------*/
OneWire oneWire(ONE_WIRE_BUS);

//Pass oneWire reference to Dallas Temperature Sensor.
DallasTemperature sensors(&oneWire);

//DEFINE LCD ANALOG CONTROL PINS ASSIGNMENT
/*-------------------------------------------------------------------------------------------------*/
#define LCD_CS A3     // Chip Select goes to Analog 3
#define LCD_CD A2     // Command/Data goes to Analog 2
#define LCD_WR A1     // LCD Write goes to Analog 1
#define LCD_RD A0     // LCD Read goes to Analog 0
#define LCD_RESET A4  // Can alternately just connect to Arduino's reset pin
#define YP A3         // must be an analog pin, use "An" notation!
#define XM A2         // must be an analog pin, use "An" notation!
#define YM 9          // can be a digital pin
#define XP 8          // can be a digital pin

//DEFINE TOUCH POINTS FOR ILI9341 PANEL
//UPDATE AFTER RUNNING CALIBRATION ROUTINE
/*-------------------------------------------------------------------------------------------------*/
#define TS_LEFT 110
#define TS_RIGHT 920
#define TS_BOT 90
#define TS_TOP 920

//Touch Pressure
/*-------------------------------------------------------------------------------------------------*/
#define MINPRESSURE 20
#define MAXPRESSURE 2000

//Screen Declaration
/*-------------------------------------------------------------------------------------------------*/
MCUFRIEND_kbv tft;

//DEFINE Colors
/*-------------------------------------------------------------------------------------------------*/
#define BLACK 0x0000       /*   0,   0,   0 */
#define NAVY 0x000F        /*   0,   0, 128 */
#define DARKGREEN 0x03E0   /*   0, 128,   0 */
#define DARKCYAN 0x03EF    /*   0, 128, 128 */
#define MAROON 0x7800      /* 128,   0,   0 */
#define PURPLE 0x780F      /* 128,   0, 128 */
#define OLIVE 0x7BE0       /* 128, 128,   0 */
#define LIGHTGREY 0xC618   /* 192, 192, 192 */
#define DARKGREY 0x7BEF    /* 128, 128, 128 */
#define BLUE 0x001F        /*   0,   0, 255 */
#define GREEN 0x07E0       /*   0, 255,   0 */
#define CYAN 0x07FF        /*   0, 255, 255 */
#define RED 0xF800         /* 255,   0,   0 */
#define MAGENTA 0xF81F     /* 255,   0, 255 */
#define YELLOW 0xFFE0      /* 255, 255,   0 */
#define WHITE 0xFFFF       /* 255, 255, 255 */
#define ORANGE 0xFD20      /* 255, 165,   0 */
#define GREENYELLOW 0xAFE5 /* 173, 255,  47 */

//DEFINE Button Size defaults
/*-------------------------------------------------------------------------------------------------*/
#define BUTTON_W 80
#define BUTTON_H 50

//TouchScreen Area Pin Declaration
/*-------------------------------------------------------------------------------------------------*/
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
unsigned long lastTouchTime = 0;  // timestamp of last registered touch
bool touchActive = false;         // current state of touch
unsigned long touchDelay = 200;   // minimum delay (ms) between touches

//Declare Button objects
/*-------------------------------------------------------------------------------------------------*/
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

//Custom Variables
/*-------------------------------------------------------------------------------------------------*/
uint16_t identifier;  //Store Screen Identifier although this is hard coded later due to Elegoo display

//Global Variables
/*-------------------------------------------------------------------------------------------------*/
//int deviceCount;                        //# of serial sensors
DeviceAddress devices[10];
byte devicesFound = 0;
unsigned long updateMillis = millis();   // Set for screen update time since last 1s update
unsigned long InfoAge;                   // Time on Info scnreen, used to reset to home after time interval. (30s)
unsigned long UpdateAge = millis();      // Time for Dwell time interval
byte _days;                              // Ease punch frequency after this many days
float TempAct;                           // Current Temperature
float TMax;                              // Record Max temp
float TMin;                              // Min temp
int TempSetPoint;                        // Set point temp
float Interval;                          // Time to next punch
byte Index;                              // Interval index counter
byte IntervalSet;                        // Set Interval per day 1,2,4,6,8
byte CurrentPage = 1;                    // Current Page indicator  "1" First Page,  "2" Second Page
int Cycles;                              // Count of all punch cycles
int SetTempDwellTime;                    // Set Time between punches of temp is exceeded
int TempDwellTime;                       // Time since last temp punch. Used to wait this time between punches of temp is exceeded
String s_UpTime;                         // String of uptime for UI
String s_PunchReason = "Reboot";         // String for punch reason
String s_OldPunchReason;                 // String for old punch reason
int px;
int py;
int pz;
byte StrokeDownTime;               // Actuator Stroke Time Down variable (s) User Configuration
byte StrokeUpTime = 15;            // Actuator Stroke Time Up variable (s)
unsigned long ShakeDelay = 10000;  // Delay before shake starts (ms)
byte SetTimeRep_UI;                // # reps to punch for a timed trigger
byte SetTempRep_UI;                // # reps to punch for a temp trigger
byte SetPunchReps;                 // Counter for FSM to check for # reps to punch when triggered
int completedCycles = 0;           // Temp counter for number of cycles

// --- Cycle Count Button Variables ---
int cycleValues[] = { 1, 3, 5, 10 };
int CycleIndex = 0;                        // Index into cycleValues array
int CycleValue = cycleValues[CycleIndex];  // Start at 5


//Array definition
/*-------------------------------------------------------------------------------------------------*/
byte PinOutputs[4] = { Sol1, Sol2, Sol3, Sol4 };      // ARRAY of 4 config pins for digital HW output pin definitions for the for Actuator Solenoids !!!
byte _IntervalSet[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };  // ARRAY for # time periods / day
byte StartOffset[6] = { 00, 10, 20, 30, 40, 50 };     // Start time offset delay array defined by _days
// const char *id[4][2] = { { "A^", "Av" }, { "B^", "Bv" }, { "C^", "Cv" }, { "D^", "Dv" } };  // Screen characters for state display
// const char *Act[] = { "A", "B", "C", "D" };

// ====== PUNCH CONTROL VARIABLES ======
enum PunchState {
  PUNCH_IDLE,
  PUNCH_DOWN,
  PUNCH_UP,
  PUNCH_SHAKE_WAIT,
  PUNCH_SHAKE
};
PunchState punchState = PUNCH_IDLE;

// Control flags
bool PunchActive = false;  // true = mid-cycle
bool CycleFlag;            // external run flag (0 stops cycles)

// --- TIMING TRACKERS ---
unsigned long punchStartTime = 0;
unsigned long upStartTime = 0;
unsigned long lastPunchCompleteTime = 0;
int currentPunchActuator = 0;

// --- SHAKE CONTROL ---
bool shakeRunning = false;
bool shakeDelayActive = false;
unsigned long shakeDelayStart = 0;
unsigned long ShakeDepth_ms = 200;  // ms down and up duration
const int NumActuators = 4;         // Total actuators
int MaxShakeCycles = 4;             // number of full shake cycles per actuator

/*-------------------------------------------------------------------------------------------------*/

void setup(void) {       // Initial Setup
  Serial.begin(115200);  // Serial Port
  uint16_t identifier = tft.readID();
  if (identifier == 0x0101 || identifier == 0x0000 || identifier == 0xFFFF) {
    identifier = 0x9341; // Fallback for your specific screen if read fails
    }
  tft.begin(identifier)
  tft.setRotation(3);    // Portrait USB top left
  sensors.begin();       // Start temp sensor
  Serial.println("Locating temp devices...");
  Serial.print("Found ");
  devicesFound = sensors.getDeviceCount();
  Serial.print(devicesFound);
  Serial.println(" devices");

  //Define Pinmodes for Ram Solenoids using PinOutputs Array 0-3
  for (byte i = 0; i <= 3; i++) {
    pinMode(PinOutputs[i], OUTPUT);
  }

  //Define screen id - MUST BE THIS for an Elegoo display
  /*-------------------------------------------------------------------------------------------------*/
  uint16_t identifier = tft.readID();
  identifier = 0x9341;

  //Get state from EEPROM or initize and set defaults incase of new board
  /*-------------------------------------------------------------------------------------------------*/

  if (EEPROM.read(20) == 255) {  //Assume no value initialize
    Serial.print("Initializing EEPROM - ");
    EEPROM.update(20, 16);  //magic value to test for changes
    EEPROM.update(0, 0);    //Run-Stop state
    EEPROM.update(1, 4);    //default IntervalSet Index
    EEPROM.update(4, 90);   //default Temp
    EEPROM.update(6, 4);    //default Time reps
    EEPROM.update(7, 4);    //default Temp Reps
    EEPROM.update(8, 30);   //default Dwell down (30s stroke)
    EEPROM.update(11, 3);   //default punch start offset (_days)
  }

  /*-----------------COPYRIGHT------------------*/
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
  /*-----------------COPYRIGHT------------------*/

  //RESTORE current settings
  /*-------------------------------------------------------------------------------------------------*/
  CycleFlag = EEPROM.read(0);             // 0 = Stopped, 1 = Running
  Index = EEPROM.read(1);                 // Restore Interval Index
  IntervalSet = _IntervalSet[Index];      // Interval range 2-24 /day from Array index
  Interval = 1440 / _IntervalSet[Index];  // Calculated from stored interval index
  TMax = EEPROM.read(2);                  // Last Max
  TMin = EEPROM.read(3);                  // Last Min
  TempSetPoint = EEPROM.read(4);          // Temp threshold for punch start
  //EEPROM 5 is uptime flag, checked later
  SetTimeRep_UI = EEPROM.read(6);     // # repeats for time
  SetTempRep_UI = EEPROM.read(7);     // # repeats for temp
  StrokeDownTime = EEPROM.read(8);    // Stroke time for Actuator Down
  SetTempDwellTime = EEPROM.read(9);  // Time Between Temp initiated Punch actions
  Cycles = EEPROM.read(10);           // Restore number of cycles
  _days = EEPROM.read(11);            // Restores start offset array index (Offset)

  //Get Current Temp for display
  /*-------------------------------------------------------------------------------------------------*/
  TempAct = GetTemp();

  /*-------------------------------------------------------------------------------------------------*/
  drawMainScreen();  //Draw Temperature Box

  //Print setup and screen
  /*-------------------------------------------------------------------------------------------------*/
  Serial.println("Punching Matilda");
  Serial.print("TFT size is ");
  Serial.print(tft.width());
  Serial.print("x");
  Serial.println(tft.height());

  // Test if power fail and uptime > 1 hour and was running
  if (EEPROM.read(5) == 1 && CycleFlag == 1) {
    Serial.println("Power fail so punch once now");
    s_PunchReason = "Power";
    SetPunchReps = SetTimeRep_UI;  // Loop # of reps
    completedCycles = 0;
    StartPunch();  // Call Punch start of set
  } else {
    EEPROM.update(5, 0);
  }  // Wasn't running so reset up time timer
}
/*-------------------------------------------------------------------------------------------------*/

void loop() {  // Main
  /*-------------------------------------------------------------------------------------------------*/
  ReadScreen();        // Check for action on screen press
  UpdatePunch();       // Run any necessary actuator actions
  UpdateShakeDelay();  // Non-blocking shake delay check

  if (millis() - UpdateAge >= 60000) {     // If 1 min passes, increase time since TEMP punch counter
    EEPROM.update(5, millis() > 3600000);  // Set flag if run time exceeds 1 hour, else it resets. Only writes if it is different
    TempDwellTime += 1;                    // Add one minute to the Temp Dwell Time
    UpdateAge = millis();                  // Reset Temp wait interval measurement
    Serial.print("Temp Timer Counter (min): ");
    Serial.println(TempDwellTime);
  }

  if ((TempAct >= TempSetPoint && TempDwellTime >= SetTempDwellTime) && !PunchActive) {  // Temp limit reached and more than Time since last Temp punch exceeded
    Serial.println("Temp Exceeded and longer than Temp Wait Period");                    // Logging
    s_PunchReason = "Temp";                                                              // Set last Punch reason
    SetPunchReps = SetTempRep_UI;                                                        // Set number of cycles required
    StartPunch();                                                                        // Call Punch start of set
    TempDwellTime = 0;                                                                   // Reset temp wait time if temp punch occurs
    Interval = 1440 / IntervalSet;                                                       // Reset time interval if temp punch occurs
  }

  // Screen update each second
  /*-------------------------------------------------------------------------------------------------*/
  if (millis() - updateMillis >= 1000) {          // 1 second interval check
    updateMillis = millis();                      // 1s Display timer
    if (CycleFlag == 1) {                         // Running
      Interval -= 0.01666667;                     // Decrease time wait punch interval if running. *** Reduce to speed time when testing - default = 0.016667
      if (Interval <= 0.02) {                     // Time limit reached
        Serial.println("Time Interval Reached");  // Log Reason
        s_PunchReason = "Time";                   // Set Last Punch reason
        Interval = 1440 / IntervalSet;            // Reset Interval without offset
        TempDwellTime = 0;                        // Reset temp wait interval if time interval reached
        SetPunchReps = SetTimeRep_UI;             // Loop # of reps for time
        StartPunch();                             // Call Punch start of set
      }
      if (s_OldPunchReason != s_PunchReason) {  // If old reason not equal to new reason, update & display
        s_OldPunchReason = s_PunchReason;       // Update old reason
        tft.fillRect(235, 222, 75, 15, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(240, 222);
        tft.println(s_OldPunchReason);
      }
    }

    if (CurrentPage != 4) {  // Always display current temp
      updateTemp();
      if (CycleFlag == 1 && CurrentPage == 1) {  //Display interval if running on page 1 (main Page)
        updateInterval();
      }
    }

    if ((CurrentPage == 2 || CurrentPage == 4 || (CurrentPage == 3 && CycleFlag == 0)) && millis() - InfoAge >= 60000) {  // Timeout for INFO & CONFIG screen after 1 min back to main
      CurrentPage = 1;
      Serial.println("Screen Timeout.. Return Home ");
      drawMainScreen();
    }
  }
  if (!PunchActive && !shakeRunning) {
    AllActuatorsUp();
  }
}
/*-------------------------------------------------------------------------------------------------*/

// FUNCTIONS
void ReadScreen() {
  if (!ScreenTouched()) return;
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

void drawConfigScreen() {  //Persistent config settings
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  //TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(10, 10);
  tft.println("Plunge Settings");
  // DIVIDERS LINES
  tft.drawLine(0, 42, 320, 42, GREEN);  //Title

  //Increment selectors
  // UP
  tft.setTextColor(GREEN);
  tft.setCursor(70, 80);  //Speed Down - Increase
  tft.println("+");
  tft.setCursor(230, 80);  //Time Cycles Increase
  tft.println("+");
  tft.setCursor(70, 177);  //Temp Dwell Increase
  tft.println("+");
  tft.setCursor(230, 177);  //Temp Cycles Increase
  tft.println("+");

  // DOWN
  tft.setTextColor(RED);
  tft.setCursor(70, 115);  //Speed Down - Decrease
  tft.println("-");
  tft.setCursor(230, 115);  //Time Cycles Decrease
  tft.println("-");
  tft.setCursor(70, 207);  //Temp Dwell Decrease
  tft.println("-");
  tft.setCursor(230, 207);  //Temp Cycles Decrease
  tft.println("-");

  //Labels
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

  //VALUES
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

  InfoAge = millis();  //Reset screen timeout counter
}
/*END----------------------------------------------------------------------------------------------*/

void updateTemp() {  //Display update of Temperature
  if (CurrentPage != 4) {
    GetTemp();
    tft.fillRect(20, 65, 70, 40, BLACK);
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(20, 75);
    tft.println(TempAct, 1);
  }
  if (CurrentPage == 3) {
    if (PunchActive) {
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 140);
      tft.print("Cycles ");
      tft.fillRect(90, 140, 20, 20, BLACK);
      tft.print(completedCycles );
      tft.print(" of ");
      tft.fillRect(160, 140, 20, 20, BLACK);
      tft.print(SetPunchReps);
      ButtonCycle.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Cycling", 1);
      ButtonCycle.drawButton(true);
    } else {
      tft.fillRect(15, 138, 180, 20, BLACK);
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

float GetTemp() {  //Call to temperature sensor
  //Get temp and update
  if (devicesFound > 0) {
    sensors.requestTemperatures();           //Send the command to get temperature readings
    TempAct = sensors.getTempFByIndex(0);    //Return result as Farenheit
    if (TempAct < -20) {                     //Possible error
      sensors.begin();                       //Try to start restart temp sensor if invalid /HACK
      sensors.requestTemperatures();         //Send the command to get temperature readings
      TempAct = sensors.getTempFByIndex(0);  //Return result as Farenheit
    }
    if (TempAct < -20) {  // Error condition, code or broken sensor
      TempAct = 0.00;     // Defeault to 0.00
    }
    if (TempAct > TMax) {
      TMax = TempAct;
      EEPROM.update(2, TMax);
    }
    if ((TempAct < TMin) && (TempAct != 0)) {
      TMin = TempAct;
      EEPROM.update(3, TMin);
    }
  }
  return TempAct;
}
/*END----------------------------------------------------------------------------------------------*/

String Uptime() {
  int time_mins = (int)floor(millis() / 60000);  //Get whole minutes
  int hours = time_mins / 60;                    //Change to *24 to have a minute = a day
  int minutes = time_mins % 60;                  //Get remainder
  String _hour = String(hours);
  String _min = String(minutes);
  //  String Time;
  if (_min.length() == 1) {  //Pad with leading zero
    _min = "0" + _min;
  }
  s_UpTime = _hour + ":" + _min;

  return s_UpTime;
}
/*END----------------------------------------------------------------------------------------------*/

void updateInterval() {         //Display Interval till update
  int hours = (Interval / 60);  //Hours
  int minutes = ((Interval / 60) - hours) * 60;
  int seconds = (((Interval / 60) - hours) * 60 - minutes) * 60;
  String s_NextTime;
  String _hour = String(hours);
  String _min = String(minutes);
  String _sec = String(seconds);
  if (_min.length() == 1) {
    _min = "0" + _min;
  }
  if (_sec.length() == 1) {
    _sec = "0" + _sec;
  }
  s_NextTime = _hour + ":" + _min + ":" + _sec;
  tft.setTextSize(2);
  tft.fillRect(125, 68, 100, 25, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 75);
  tft.println(s_NextTime);  //Time to next Punch
  tft.setTextSize(2);
  tft.fillRect(125, 115, 60, 20, BLACK);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 117);
  tft.println(TempDwellTime);  //Time to next Punch
}
/*END----------------------------------------------------------------------------------------------*/

void UpdateSetInterval() {
  tft.setTextSize(3);
  tft.setCursor(125, 175);
  tft.fillRect(110, 170, 60, 40, BLACK);  // Clear per day
  tft.println(_IntervalSet[Index]);
  updateInterval();
}
/*END----------------------------------------------------------------------------------------------*/

void drawhomeicon() {  // draws a white home icon
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

bool ScreenTouched() {
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  // Get x,y&z values and transpose (There is a better way than this but this works)
  p.x = (tft.height() - map(p.x, TS_LEFT, TS_RIGHT, tft.height(), 0));  //Scale range to adjust to rotation from Portrait to landscape
  p.y = (tft.width() - map(p.y, TS_BOT, TS_TOP, tft.width(), 0));
  px = p.y;
  py = p.x;
  pz = p.z;
  bool isTouched = (pz != 0 && ((pz > MINPRESSURE && pz < MAXPRESSURE) || (px > 100 && px < 900 && py > 100 && py < 900)));

  unsigned long now = millis();

  // If still touching, hold state but don't repeat
  if (isTouched && !touchActive && (now - lastTouchTime >= touchDelay)) {
    touchActive = true;
    lastTouchTime = now;
    Serial.print("x:");
    Serial.print(px);
    Serial.print(", y:");
    Serial.print(py);
    Serial.print(", z:");
    Serial.println(pz);
    return true;  // New touch event registered
  }

  // When released, reset touch state so next press can register
  if (!isTouched) {  //} && touchActive) {
                     //  if (now - lastTouchTime > 100) {  // 100ms release debounce
    touchActive = false;
    //  }
  }
  return false;  // No new touch detected
}
/*END----------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------*/
/*Abort Punch e.g. when cycle is stopped-----------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------*/
void AbortPunch() {
  Serial.println("Reset Punch!");
  CycleFlag = 0;                    // Stop running cycles
  currentPunchActuator = 0;
  completedCycles = SetPunchReps;
  PunchActive = false;
  shakeRunning = false;
  punchState = PUNCH_SHAKE_WAIT;    // Set at Punch complete stage
  lastPunchCompleteTime = millis(); // Used to set shake delay timer
  AllActuatorsUp();                 // ensure all are retracted

  Serial.println("Running shake routine after abort...");
  StartShake();  // ✅ Force a shake after abort
}
/*END----------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------*/
/*Punch Routine based on State model and count defined by SetTimeRep or SetTempRep-----------------*/
void StartPunch() {
  if (PunchActive) return;
  completedCycles = 0;
  PunchActive = true;
  punchState = PUNCH_DOWN;
  punchStartTime = millis();
  currentPunchActuator = 0;
  Serial.print("Starting punch sequence. On ");
  Serial.print(completedCycles + 1);
  Serial.print(" of ");
  Serial.println(SetPunchReps);

  if (CurrentPage == 3) {
    tft.setTextSize(2);
    tft.setCursor(15, 140);
    tft.print("Cycles ");
    tft.print(completedCycles + 1);
    tft.print(" of ");
    tft.print(SetPunchReps);
  }
}
/*END----------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------*/
/*Update punch ------------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------*/
void UpdatePunch() {
  if (!PunchActive) return;
  unsigned long now = millis();

  switch (punchState) {

    // --- ACTUATOR DOWN ---
    case PUNCH_DOWN:
      digitalWrite(PinOutputs[currentPunchActuator], HIGH);
      if (now - punchStartTime >= StrokeDownTime * 1000) {
        digitalWrite(PinOutputs[currentPunchActuator], LOW);
        upStartTime = now;
        punchState = PUNCH_UP;
        Serial.print("Actuator ");
        Serial.print(currentPunchActuator + 1);
        Serial.println(" down complete, moving up");
      }
      break;

    // --- ACTUATOR UP ---
    case PUNCH_UP:
      if (now - upStartTime >= StrokeUpTime * 1000) {
        currentPunchActuator++;
        if (currentPunchActuator < 4) {
          punchStartTime = now;
          punchState = PUNCH_DOWN;
          Serial.print("Starting actuator ");
          Serial.println(currentPunchActuator + 1);
        } else {
          completedCycles++;  //Update cycle Count
          punchState = PUNCH_SHAKE_WAIT;
          lastPunchCompleteTime = now;
          Serial.println("All actuators complete, waiting for shake delay");
          Cycles += 1;
          EEPROM.update(10, Cycles);
          lastPunchCompleteTime = millis();
        }
      }
      break;

    // --- WAIT BEFORE SHAKE ---
    case PUNCH_SHAKE_WAIT:
      if (now - lastPunchCompleteTime >= ShakeDelay) {
        Serial.println("Punch Wait Competed");
        StartShake();
        punchState = PUNCH_SHAKE;
        Serial.println("Shake triggered after delay");
      }
      break;

    // --- SHAKE ACTIVE ---
    case PUNCH_SHAKE:
      if (!shakeRunning) {
        RunBlockingShake();
        punchState = PUNCH_IDLE;
        PunchActive = false;
        AllActuatorsUp();  // ensure all actuators are retracted
        lastPunchCompleteTime = millis();
        Serial.print("Punch and shake complete, temp lockout for ");
        Serial.print(SetTempDwellTime);
        Serial.println(" minutes");
      }
      break;
  }
}

/*----------------------------------------------------------------------------------------*/
/* SHAKE ROUTINE HERE - Tried State model but timing was too inconsistent for a good shake*/
/* See Version 25.10.15.FSM---------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------*/
/* StartShake() — initiates a non-blocking delay before shake begins                               */
/*-------------------------------------------------------------------------------------------------*/
void StartShake() {
  if (shakeDelayActive || shakeRunning || PunchActive) return;  // prevent overlap

  unsigned long now = millis();
  unsigned long timeSincePunch = now - lastPunchCompleteTime;

  if (timeSincePunch >= ShakeDelay) {
    // Enough time has already passed — skip delay
    Serial.println("Shake delay skipped — starting immediately...");
    RunBlockingShake();
  } else {
    // Wait remaining time until delay expires
    shakeDelayActive = true;
    shakeDelayStart = now;
    Serial.print("Shake delay started (remaining ");
    Serial.print(ShakeDelay - timeSincePunch);
    Serial.println(" ms) before shake begins.");
  }
}

/*-------------------------------------------------------------------------------------------------*/
/* UpdateShakeDelay() — must be called from loop() to monitor delay expiry                         */
/*-------------------------------------------------------------------------------------------------*/
void UpdateShakeDelay() {
  if (shakeDelayActive && millis() - shakeDelayStart >= ShakeDelay) {
    shakeDelayActive = false;
    Serial.println("Shake delay complete — starting blocking shake...");
    RunBlockingShake();  // <--- This part will block until shake completes
  }
}

/*-------------------------------------------------------------------------------------------------*/
/* RunBlockingShake() — performs the actual blocking shake sequence                                */
/*-------------------------------------------------------------------------------------------------*/
void RunBlockingShake() {
  shakeRunning = true;
  for (int actuator = 0; actuator < NumActuators; actuator++) {
    if (CurrentPage == 3) {
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 160);
      tft.println("Shaking #");
      Serial.print("Shaking actuator ");
      Serial.println(actuator + 1);
      tft.setCursor(130, 160);
      tft.fillRect(130, 160, 20, 20, BLACK);
      tft.println(actuator + 1);
    }

    for (int cycle = 0; cycle < MaxShakeCycles; cycle++) {
      digitalWrite(PinOutputs[actuator], HIGH);  // Move DOWN
      delay(ShakeDepth_ms);
      digitalWrite(PinOutputs[actuator], LOW);  // Move UP
      delay(ShakeDepth_ms);
    }
    if (CurrentPage == 3 && actuator == 3) {
      ButtonCycle.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
      ButtonCycle.drawButton(true);
    }
  }
  if (CurrentPage == 3) {
    tft.fillRect(15, 160, 140, 20, BLACK);
  }
  Serial.println("Shake routine complete.");
  shakeRunning = false;
}
/*END----------------------------------------------------------------------------------------------*/

/* Screen Handlers*/
void handleTempAdjust() {
  if (px >= 60 && px < 150 && py >= 120 && py <= 240 && CurrentPage == 1) {
    if (py <= 200 && TempSetPoint < 120) {
      TempSetPoint++;
    } else if (py > 200 && TempSetPoint > 50) {
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

void handleIntervalAdjust() {
  if (px >= 150 && px <= 220 && py >= 120 && py <= 240 && CurrentPage == 1) {
    if (py <= 200 && Index < 7) {
      Index++;
    } else if (py > 200 && Index > 0) {
      Index--;
    }
    EEPROM.update(1, Index);
    IntervalSet = _IntervalSet[Index];
    Interval = 1440 / IntervalSet + _days * 10;
    UpdateSetInterval();
    updateInterval();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleRunStop() {
  if (CurrentPage == 1 && px >= 235 && px <= 305 && py >= 50 && py <= 100) {
    CycleFlag = !CycleFlag;
    EEPROM.update(0, CycleFlag);
    if (CycleFlag == 0) {
      ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
      AbortPunch();
    } else {
      ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Pause", 2);
    }
    ButtonState.drawButton(true);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleManualCycle() {
  if (px >= 235 && px <= 305 && py >= 110 && py <= 160) {
    switch (CurrentPage) {
      case 1:
        CurrentPage = 3;
        CycleFlag = 0;
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
        ButtonCount.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, labelBuffer, 2);
        ButtonCount.drawButton(true);
        break;
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleInfoPage() {
  if (CurrentPage == 1 && px >= 250 && px <= 320 && py >= 180 && py <= 240) {
    CurrentPage = 2;
    drawInfoScreen();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleManualCycleActions() {
  if (CurrentPage == 3) {
    if (px >= 235 && px <= 320 && py >= 50 && py <= 100) {
      CycleFlag = !CycleFlag;
      if (CycleFlag == 0 && !PunchActive) {
        ButtonCycle.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
        ButtonCycle.drawButton(true);
        AbortPunch();
      } else {
        s_PunchReason = "Manual";
        SetPunchReps = CycleValue;
        completedCycles = 0;
        StartPunch();
      }
    }
    if (px >= 235 && px <= 320 && py >= 180 && py <= 240 && !PunchActive) {
      StartShake();
    }
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleStatusReset() {
  if (px >= 235 && px <= 305 && py >= 50 && py <= 100 && CurrentPage == 2) {
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

void handleOffsetAdjust() {
  if (px >= 100 && px < 220 && py >= 150 && py <= 240 && CurrentPage == 2) {
    if (px < 150 && _days > 0) {
      _days--;
    } else if (px >= 150 && _days < 5) {
      _days++;
    }
    EEPROM.update(11, _days);
    tft.fillRect(150, 197, 35, 30, BLACK);
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(150, 200);
    tft.println(StartOffset[_days]);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleConfigAdjustments() {
  if (CurrentPage != 4) return;

  // Stroke Down Time
  if (px >= 20 && px <= 180 && py >= 30 && py <= 160) {
    if (py < 100 && StrokeDownTime < 90) StrokeDownTime += 5;
    else if (py >= 100 && StrokeDownTime > 5) StrokeDownTime -= 5;
    EEPROM.update(8, StrokeDownTime);
    tft.fillRect(10, 90, 40, 35, BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 95);
    tft.println(StrokeDownTime);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Time Reps
  if (px >= 180 && px <= 280 && py >= 30 && py <= 160) {
    if (py < 100 && SetTimeRep_UI < 10) SetTimeRep_UI++;
    else if (py >= 100 && SetTimeRep_UI > 1) SetTimeRep_UI--;
    EEPROM.update(6, SetTimeRep_UI);
    tft.fillRect(170, 90, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(180, 95);
    tft.println(SetTimeRep_UI);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Temp Reps
  if (px >= 180 && px <= 280 && py >= 170 && py <= 235) {
    if (py < 200 && SetTempRep_UI < 10) SetTempRep_UI++;
    else if (py >= 200 && SetTempRep_UI > 1) SetTempRep_UI--;
    EEPROM.update(7, SetTempRep_UI);
    tft.fillRect(170, 180, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(180, 190);
    tft.println(SetTempRep_UI);
  }
  /*END----------------------------------------------------------------------------------------------*/

  // Temp Dwell
  if (px >= 20 && px <= 180 && py > 170 && py <= 235) {
    if (py < 200 && SetTempDwellTime < 240) SetTempDwellTime += 5;
    else if (py >= 200 && SetTempDwellTime > 0) SetTempDwellTime -= 5;
    EEPROM.update(9, SetTempDwellTime);
    tft.fillRect(15, 190, 60, 40, BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 190);
    tft.println(SetTempDwellTime);
  }
}
/*END----------------------------------------------------------------------------------------------*/

void handleHomeIcon() {
  if (px >= 260 && px <= 320 && py >= 0 && py <= 40 && CurrentPage != 1) {
    CurrentPage = 1;
    drawMainScreen();
  }
}
/*END----------------------------------------------------------------------------------------------*/

void drawMainScreen() {  //Main operating screen
  touchActive = false;
  tft.fillScreen(BLACK);  //Clear screen
  Serial.print("Time since last punch = ");
  Serial.print((millis() - lastPunchCompleteTime) / 1000);
  Serial.println(" s");
  //TITLE
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(15, 10);
  tft.println("Satori Cellars");
  tft.drawLine(10, 42, 320, 42, GREEN);

  //Labels
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
  tft.println("Temp(m)");  //Time to next Punch

  //Incrementors
  tft.setTextSize(3);
  tft.setTextColor(GREEN);  //UP
  tft.setCursor(80, 160);   //Temp UP
  tft.println("+");
  tft.setCursor(190, 160);  //Interval UP
  tft.println("+");

  tft.setTextColor(RED);   //DOWN
  tft.setCursor(80, 200);  //Temp DOWN
  tft.println("-");
  tft.setCursor(190, 200);  //Interval Down
  tft.println("-");

  //Data
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();
  tft.setCursor(20, 175);
  tft.println(TempSetPoint);
  updateInterval();
  UpdateSetInterval();
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(0, 222);
  tft.println("Last Punch Reason:>");
  tft.setTextColor(RED);
  tft.setCursor(240, 222);
  tft.println(s_PunchReason);

  //BUTTONS
  //Stop Go
  if (CycleFlag == 0) {
    ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
  } else {
    ButtonState.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Pause", 2);
  }
  ButtonState.drawButton(true);

  //Manual
  ButtonPunch.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, (char *)"Cycle", 2);
  ButtonPunch.drawButton(true);

  //INFO
  ButtonInfo.initButton(&tft, 280, 195, BUTTON_W, BUTTON_H, WHITE, WHITE, DARKGREY, (char *)"Status", 2);
  ButtonInfo.drawButton(true);
}
/*END----------------------------------------------------------------------------------------------*/

void drawManualScreen() {  //Manual Punch Screen
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  tft.setCursor(5, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Cycle Punches");
  // DIVIDER LINES
  tft.drawLine(10, 42, 320, 42, GREEN);  //Title

  // tft.drawLine(215, 42, 215, 230, GREEN);  //Vertical Center

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(15, 50);
  tft.println("Temp(F)");
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  updateTemp();

  if (CycleFlag == 0) {
    //Cycle State
    ButtonCycle.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, BLACK, GREEN, (char *)"Run", 2);
    ButtonCycle.drawButton(true);
  } else {
    ButtonCycle.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Cycling", 2);
    ButtonCycle.drawButton(true);
  }

  // Update button label
  static char labelBuffer[5];
  sprintf(labelBuffer, "%d", CycleValue);

  //Cycle Count
  ButtonCount.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, (char *)labelBuffer, 2);
  ButtonCount.drawButton(true);

  //Shake
  ButtonShake.initButton(&tft, 280, 195, BUTTON_W, BUTTON_H, WHITE, BLACK, ORANGE, (char *)"Shake", 2);
  ButtonShake.drawButton(true);
}
/*END----------------------------------------------------------------------------------------------*/

void drawInfoScreen() {  //Running status screen
  touchActive = false;
  tft.fillScreen(BLACK);
  drawhomeicon();
  //TITLE
  tft.setCursor(15, 10);
  tft.setTextColor(RED);
  tft.setTextSize(3);
  tft.println("Status");
  // DIVIDERS LINES
  tft.drawLine(10, 42, 320, 42, GREEN);  //Title
  tft.drawLine(115, 45, 115, 240, GREEN);

  //TEMP
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  //TITLES
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
  tft.println("Start Delay");

  tft.setCursor(230, 205);
  tft.println("mins");

  //VALUES
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

  tft.setCursor(150, 200);
  tft.println(StartOffset[_days]);

  //Reduce Interval Down
  tft.setTextSize(3);
  tft.setTextColor(RED);
  tft.setCursor(125, 200);
  tft.println("-");

  //Increase Interval Up
  tft.setTextColor(GREEN);
  tft.setCursor(200, 200);
  tft.println("+");

  //BUTTONS
  //Reset
  ButtonReset.initButton(&tft, 280, 75, BUTTON_W, BUTTON_H, WHITE, WHITE, RED, (char *)"Reset", 2);
  ButtonReset.drawButton(true);
  //Manual
  ButtonConfig.initButton(&tft, 280, 135, BUTTON_W, BUTTON_H, WHITE, WHITE, BLUE, (char *)"Config", 2);
  ButtonConfig.drawButton(true);
  InfoAge = millis();
}
/*END----------------------------------------------------------------------------------------------*/


void AllActuatorsUp() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(PinOutputs[i], LOW);
  }
}