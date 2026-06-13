#pragma once

#if defined(DEVICE_WAVESHARE_S3_GEEK)

#include <Arduino.h>
#include "Inputs/InputKeys.h"
#include <Interfaces/IDeviceView.h>
//#include "Interfaces/IInput.h"

#define NVS_SSID_KEY "ssid"
#define NVS_PASS_KEY "pass"

#define DARK_GREY 0x4208
#define HELP_COLOR 0xC618

bool setupWaveshareS3GeekWifi(IDeviceView& view);

#endif
