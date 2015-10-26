
#ifndef __CONFIG_H__
#define __CONFIG_H__

//#define DEBUG_OLED_DISPLAY

#define FIRMWARE_MAJOR_VERSION 3
#define FIREWARM_MINOR_VERSION 1

#define FW_STRING_LEN 25

#define DEFAULT_HIGH_TEMP_THRESHOLD 3900
#define MIN_TEMP_THRESHOLD 3600
#define MAX_TEMP_THRESHOLD 4200

#define PRE_RELEASE

#define USE_6448_DISPLAY
#ifdef USE_6448_DISPLAY

#define CONFIG_USE_6448_DISPLAY
#include "ther_oled6448_drv.h"
#include "ther_oled6448_display.h"

#else

#define CONFIG_USE_9639_DISPLAY
#include "ther_oled9639_drv.h"
#include "ther_oled9639_display.h"
#endif

#endif

