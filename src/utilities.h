#pragma once

#include "TelnetSpy.h"

// create instance of telnet/serial wrapper
extern TelnetSpy SerialAndTelnet;

#undef SERIAL
//#define SERIAL  Serial
#define SERIAL  SerialAndTelnet

//
// printf macro
//

#define LOG_PRINTF(fmt, ...) printf_internal(PSTR(fmt), ##__VA_ARGS__)
void printf_internal(const char *fmt, ...);

char *msToTimeStr(unsigned long ms);
void watchdogReset();
