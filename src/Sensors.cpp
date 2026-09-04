#include "Sensors.h"
#include <EEPROM.h>

// --- HARDWARE INSTANTIATIONS ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
byte devicesFound = 0;

void initSensors() {
    sensors.begin();
    Serial.println("Locating temp devices...");
    Serial.print("Found ");
    devicesFound = sensors.getDeviceCount();
    Serial.print(devicesFound);
    Serial.println(" devices");
}

float GetTemp() {
  if (devicesFound > 0) {
    sensors.requestTemperatures();           
    TempAct = sensors.getTempFByIndex(0);    
    if (TempAct < -20) {                     
      sensors.begin();                       
      sensors.requestTemperatures();         
      TempAct = sensors.getTempFByIndex(0);  
    }
    if (TempAct < -20) {  
      TempAct = 0.00;     
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