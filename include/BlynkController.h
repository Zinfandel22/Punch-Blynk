#pragma once

void beginBlynkController();
void scheduleBlynkTimer(unsigned long interval, void (*callback)());
void runBlynkController();
void beginBlynkEdgent();
bool blynkProvisioningActive();
void publishBlynkState();
void requestBlynkDisplayRefresh();
void publishActuatorStateIfChanged();
void publishCycleValue();
