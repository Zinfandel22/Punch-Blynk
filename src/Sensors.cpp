#include "Sensors.h"
#include <EEPROM.h>

// --- HARDWARE INSTANTIATIONS ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
byte devicesFound = 0;
bool conversionPending = false;

void initSensors() {
    sensors.begin();
  sensors.setWaitForConversion(false);
    Serial.println("Locating temp devices...");
    Serial.print("Found ");
    devicesFound = sensors.getDeviceCount();
    Serial.print(devicesFound);
    Serial.println(" devices");
}

float GetTemp() {
  if (devicesFound == 0)
    return TempAct;

  if (!conversionPending) {
    sensors.requestTemperatures();
    conversionPending = true;
    return TempAct;
  }

  if (!sensors.isConversionComplete())
    return TempAct;

  const float newTemperature = sensors.getTempFByIndex(0);
  sensors.requestTemperatures();
  conversionPending = true;

  if (newTemperature < -20)
    return TempAct;

  TempAct = newTemperature;
  if ((TempAct != TMax) && (TempAct > TMax)) {
    TMax = TempAct;
    EEPROM.update(2, TMax);
  }
  if ((TempAct != TMin) && (TempAct < TMin) && (TempAct != 0)) {
    TMin = TempAct;
    EEPROM.update(3, TMin);
  }
  return TempAct;
}