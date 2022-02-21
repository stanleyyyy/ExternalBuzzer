//
// Device: M5Stamp (BUILD_PICO_STAMP == 1) or M5Stack Atom Lite (BUILD_PICO_STAMP == 0)
//

#if BUILD_PICO_STAMP
#include "Arduino.h"
#include <FastLED.h>
#else
#include "M5Atom.h"
#endif

#include "config.h"
#include "SerialAndTelnetInit.h"
#include "watchdog.h"
#include "serverTask.h"
#include "ntpTask.h"
#include "otaTask.h"
#include "beeperTask.h"

void setup()
{
	//
	// make sure wifi is initialized before calling anything else
	//

	WiFi.mode(WIFI_OFF);
	WiFi.mode(WIFI_MODE_STA);
	WiFi.setSleep(false);

	// init serial/telnet
	SerialAndTelnetInit::init();

	// init watchdog
	watchdogInit();

#if	(BUILD_PICO_STAMP == 0)
	// Init M5Atom
	M5.begin(false, true, true);
	delay(10);
#endif

#if 0
	// wait until serial is available
	while (!Serial) {};
#endif

	//
	// start beeper task
	//

	xTaskCreatePinnedToCore(
		beeperTask,
		"beeperTask",	// Task name
		8192,			 // Stack size (bytes)
		NULL,			 // Parameter
		1,				 // Task priority
		NULL,			 // Task handle
		ARDUINO_RUNNING_CORE);

	//
	// start OTA task
	//

	xTaskCreatePinnedToCore(
		otaTask,
		"otaTask",		 // Task name
		8192,			 // Stack size (bytes)
		NULL,			 // Parameter
		1,				 // Task priority
		NULL,			 // Task handle
		ARDUINO_RUNNING_CORE);

	//
	// Connect to WiFi & keep the connection alive.
	//

	xTaskCreatePinnedToCore(
		wifiTask,
		"wifiTask", // Task name
		8192,			 // Stack size (bytes)
		NULL,			 // Parameter
		2,				 // Task priority
		NULL,			 // Task handle
		ARDUINO_RUNNING_CORE);

	//
	// wait for connection
	//

	wifiWaitForConnection();

	//
	// now start the server task and NTP task
	//

	xTaskCreatePinnedToCore(
		serverTask,
		"serverTask",	 // Task name
		8192,			 // Stack size (bytes)
		NULL,			 // Parameter
		2,				 // Task priority
		NULL,			 // Task handle
		ARDUINO_RUNNING_CORE);

#if NTP_TIME_SYNC_ENABLED == true
	//
	// Update time from NTP server.
	//

	xTaskCreate(
		fetchTimeFromNTP,
		"Update NTP time",
		5000, // Stack size (bytes)
		NULL, // Parameter
		1,	  // Task priority
		NULL  // Task handle
	);
#endif
}

void loop()
{
	delay(500);
}

// handle diagnostic informations given by assertion and abort program execution:
void __assert(const char *__func, const char *__file, int __lineno, const char *__sexp)
{
	// transmit diagnostic informations through serial link.
	SERIAL.println(__func);
	SERIAL.println(__file);
	SERIAL.println(__lineno, DEC);
	SERIAL.println(__sexp);
	SERIAL.flush();
	// abort program execution.
	abort();
}
