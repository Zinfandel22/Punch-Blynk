#include <Arduino.h>
#include "Config.h"
#include "UI.h"
#include <EEPROM.h>
#include "Sensors.h"
#include "Actuators.h"
#include <SPI.h>  
#include <Wire.h> 

extern MCUFRIEND_kbv tft;
extern Adafruit_GFX_Button ButtonCycle;

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>

void AllActuatorsUp();
void StartPunch();

uint16_t identifier; 

DeviceAddress devices[10];
unsigned long updateMillis = millis(); 
unsigned long InfoAge;                 
unsigned long UpdateAge = millis();    
byte _days;                            
float TempAct;                         
float TMax;                            
float TMin;                            
int TempSetPoint;                      
float Interval;                        
byte Index;                            
byte IntervalSet;                      
byte CurrentPage = 1;                  
int Cycles;                            
int SetTempDwellTime;                  
int TempDwellTime;                     
String s_UpTime;                       
String s_PunchReason = "Reboot";       
String s_OldPunchReason;               
byte StrokeDownTime;              
byte StrokeUpTime = 15;           
unsigned long ShakeDelay = 10000; 
byte SetTimeRep_UI;               
byte SetTempRep_UI;               
byte SetPunchReps;                
int completedCycles = 0;          

int cycleValues[] = {1, 3, 5, 10};
int CycleIndex = 0;                       
int CycleValue = cycleValues[CycleIndex]; 

byte PinOutputs[4] = {Sol1, Sol2, Sol3, Sol4};     
byte _IntervalSet[8] = {1, 2, 3, 4, 6, 8, 12, 24}; 
byte StartOffset[6] = {00, 10, 20, 30, 40, 50};    

// --- GLOBALS EXPOSED FOR UI.CPP ---
bool PunchActive = false; 
bool AutoCycleEnabled;
int currentPunchActuatorGlobal = 0;
unsigned long lastPunchCompleteTime = 0; 
bool shakeRunning = false; // Legacy fallback

unsigned long ShakeDepth_ms = 200; 
int MaxShakeCycles = 4;        

// --- WRAPPER FOR UI.CPP ---
void StartShake() {
    actuators.forceShake();
}

void setup(void)
{                       
  Serial.begin(115200); 
  if (identifier == 0x0101 || identifier == 0x0000 || identifier == 0xFFFF) {
    identifier = 0x9341; 
  }
  tft.begin(identifier);
  tft.setRotation(1); 
  initDisplay();
  initSensors();
  actuators.begin();

  for (byte i = 0; i <= 3; i++) {
    pinMode(PinOutputs[i], OUTPUT);
  }

  if (EEPROM.read(20) == 255) { 
    EEPROM.update(20, 16); 
    EEPROM.update(0, 0);   
    EEPROM.update(1, 4);   
    EEPROM.update(4, 90);  
    EEPROM.update(6, 4);   
    EEPROM.update(7, 4);   
    EEPROM.update(8, 30);  
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

  if (EEPROM.read(5) == 1 && AutoCycleEnabled == 1) {
    s_PunchReason = "Power";
    SetPunchReps = SetTimeRep_UI; 
    completedCycles = 0;
    StartPunch(); 
  } else {
    EEPROM.update(5, 0);
  } 
}

void loop()
{ 
  ReadScreen();       
  actuators.update(); 

  if (millis() - UpdateAge >= 60000) {                                       
    EEPROM.update(5, millis() > 3600000); 
    TempDwellTime += 1;                   
    UpdateAge = millis();                 
  }

  if ((TempAct >= TempSetPoint && TempDwellTime >= SetTempDwellTime) && !PunchActive) {                                                                   
    s_PunchReason = "Temp";                                           
    SetPunchReps = SetTempRep_UI;                                     
    StartPunch();                                           
    TempDwellTime = 0;                                                
    Interval = 1440 / IntervalSet;                                    
  }

  if (millis() - updateMillis >= 1000) {                          
    updateMillis = millis(); 
    if (AutoCycleEnabled == 1) {
      Interval -= 0.01666667; 
      if (Interval <= 0.02) {                                          
        s_PunchReason = "Time";                  
        Interval = 1440 / IntervalSet;           
        TempDwellTime = 0;                       
        SetPunchReps = SetTimeRep_UI;            
        StartPunch();                  
      }
      if (s_OldPunchReason != s_PunchReason) {                                   
        s_OldPunchReason = s_PunchReason; 
        tft.fillRect(235, 222, 75, 15, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(240, 222);
        tft.println(s_OldPunchReason);
      }
    }

    if (CurrentPage != 4) { 
      updateTemp();
      if (AutoCycleEnabled == 1 && CurrentPage == 1) {
        updateInterval();
      }
    }

    if ((CurrentPage == 2 || CurrentPage == 4 || (CurrentPage == 3 && AutoCycleEnabled == 0)) && millis() - InfoAge >= 60000) {
      CurrentPage = 1;
      drawMainScreen();
    }
  }

  if (!PunchActive && actuators.getState() == PUNCH_IDLE) {
    AllActuatorsUp();
  }
}

String Uptime() {
  int time_mins = (int)floor(millis() / 60000); 
  int hours = time_mins / 60;                   
  int minutes = time_mins % 60;                 
  String _hour = String(hours);
  String _min = String(minutes);
  if (_min.length() == 1) { 
    _min = "0" + _min;
  }
  s_UpTime = _hour + ":" + _min;
  return s_UpTime;
}

void AbortPunch() {
  Serial.println("Reset Punch!");
  currentPunchActuatorGlobal = 0;
  completedCycles = SetPunchReps;
  actuators.abortPunch();
  
  Serial.println("Running shake routine after abort...");
  actuators.abortAndShake();
}

void StartPunch() {
  actuators.startPunch();
}

void AllActuatorsUp() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(PinOutputs[i], LOW);
  }
}