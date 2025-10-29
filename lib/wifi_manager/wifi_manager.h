#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifi_setup();
void startAPAlways();
void tryConnectSavedWifi(bool asyncRetry);
void goOffline();
bool isStaConnected();

#endif // WIFI_MANAGER_H
