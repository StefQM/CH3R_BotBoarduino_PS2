#include "Arduino.h"
#include "SoftwareSerial.h"
#include "PS2X_lib.h"

#include "BotLight.h"

uint32_t g_mock_millis = 0;
uint32_t g_mock_micros = 0;
uint8_t g_mock_port = 0;

MockSerial Serial;
std::string SoftwareSerial::lastOutput = "";
BotLightController g_BotLight;
