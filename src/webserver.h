#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <NTPClient.h> // Include for NTPClient access
#include <WiFiManager.h> // Include for WiFiManager access

void webserverInit();
void webserverHandle();

extern ESP8266WebServer server;
extern int currentBrightness;
extern int currentTheme;
extern String currentImage;
extern NTPClient timeClient; // Declare NTPClient object as extern
extern WiFiManager wifiManager; // Declare WiFiManager object as extern

#endif
