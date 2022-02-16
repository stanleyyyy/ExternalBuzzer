//
// Device: M5Stamp (BUILD_PICO_STAMP == 1) or M5Stack Atom Lite (BUILD_PICO_STAMP == 0)
//

#if BUILD_PICO_STAMP
#include "Arduino.h"
#include <FastLED.h>
#else
#include "M5Atom.h"
#endif

#include <WiFi.h>
#include <wiring_private.h>

#include <SensirionI2CScd4x.h>
#include <Wire.h>

#include <ArduinoJson.h>
#include <map>
#include <functional>

#include "WebServer.h"
#include <ESPAsyncWebServer.h>

// this needs PathVariableHandlers library
#include <UrlTokenBindings.h>
#include <RichHttpServer.h>
#include <ArduinoOTA.h>

#include "Wireless.h"
#include "definitions.h"
#include "utilities.h"

#include "pitches.h"
#include "button.h"

//
// ring melody
//

int ringMelody[] = {
	NOTE_C7, NOTE_D7, NOTE_E7, NOTE_F7};

int ringDurations[] = {
	30, 10, 30, 10};

#define NUM_RING_REPEATS 50

//
// alarm signals
//

// 2 short beeps per second
int melody1[] = {
	BEEP_NOTE, -1, BEEP_NOTE, -1};

// 4 short beeps per second
int melody2[] = {
	BEEP_NOTE, -1, BEEP_NOTE, -1, BEEP_NOTE, -1, BEEP_NOTE, -1};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations1[] = {
	40, 40, 40, 840};

int noteDurations2[] = {
	40, 40, 40, 40, 40, 40, 40, 720};

#define NUM_RING_NOTES (sizeof(ringMelody) / sizeof(ringMelody[0]))
#define NUM_RING_DURATIONS (sizeof(ringDurations) / sizeof(ringDurations[0]))

#define NUM_NOTES1 (sizeof(melody1) / sizeof(melody1[0]))
#define NUM_DURATIONS1 (sizeof(noteDurations1) / sizeof(noteDurations1[0]))

#define NUM_NOTES2 (sizeof(melody2) / sizeof(melody2[0]))
#define NUM_DURATIONS2 (sizeof(noteDurations2) / sizeof(noteDurations2[0]))

#define NUM_MELODIES 2

#define SPEED 1.0

//
// global object
//

using namespace std::placeholders;
using RichHttpConfig = RichHttp::Generics::Configs::AsyncWebServer;
using RequestContext = RichHttpConfig::RequestContextType;

class Context {
public:
	SemaphoreHandle_t m_mutex = NULL;
	uint32_t m_watchdogResetTs = 0;
	uint32_t m_periodicResetTs = 0;
	bool m_watchdogReboot = false;

	// http server
	SimpleAuthProvider m_authProvider;
	RichHttpServer<RichHttpConfig> m_server;
	SensirionI2CScd4x m_scd4x;

	bool m_ledOn = false;
	uint8_t m_brightness = 0;
	uint32_t m_color = COLOR_RED;

	int32_t m_ledCycle = 0;
	uint32_t m_lastMillis = 0;

	bool alarmRunning = false;
	bool bellRunning = false;

	unsigned int melodyStart = 0;
	unsigned int melodyPos = 0;
	unsigned int notePos = 0;
	int repeatCnt = 0;

	bool m_bellOn;
	bool m_alarmOn;

#if BUILD_PICO_STAMP
	// Define the array of leds
	CRGB m_leds[NUM_LEDS];
#endif

	Context()
	: m_mutex(NULL)
	, m_watchdogResetTs(0)
	, m_periodicResetTs(0)
	, m_watchdogReboot(false)
	, m_server(80, m_authProvider)
	, m_ledOn(true)
	, m_brightness(BRIGHTNESS)
	, m_color(COLOR_RED)
	, m_ledCycle(0)
	, m_lastMillis(0)
	, m_bellOn(false)
	, m_alarmOn(false)
	{
	}

} g_ctx;

// create instance of telnet/serial wrapper
TelnetSpy SerialAndTelnet;

Button g_bellButton(INPUT_BELL_PIN, true, 10);

//
// Watchdog task
//

uint32_t timeToReset()
{
	return PERIODIC_RESET_TIMEOUT - (millis() - g_ctx.m_periodicResetTs);
}

//
// HTTP handlers
//

void watchdogReset()
{
	if (xSemaphoreTake(g_ctx.m_mutex, (TickType_t)5) == pdTRUE) {
		g_ctx.m_watchdogResetTs = millis();
		xSemaphoreGive(g_ctx.m_mutex);
	}
}

void watchdogScheduleReboot()
{
	if (xSemaphoreTake(g_ctx.m_mutex, (TickType_t)5) == pdTRUE) {
		g_ctx.m_watchdogReboot = true;
		xSemaphoreGive(g_ctx.m_mutex);
	}
}

void WatchdogTask(void *pvParameters __attribute__((unused)))
{
	while (1) {

		uint32_t diffMs = 0;
		if (xSemaphoreTake(g_ctx.m_mutex, (TickType_t)5) == pdTRUE) {
			diffMs = millis() - g_ctx.m_watchdogResetTs;
			xSemaphoreGive(g_ctx.m_mutex);
		}

		if (g_ctx.m_watchdogReboot) {
			LOG_PRINTF("Reboot scheduled, resetting the board!\n");
			delay(2000);
			ESP.restart();
		}

		if (diffMs > WATCHDOG_TIMEOUT){
			LOG_PRINTF("Watchddog timeout elapsed, resetting the board!\n");
			delay(2000);
			ESP.restart();
		}

		uint32_t periodicResetDiff = millis() - g_ctx.m_periodicResetTs;

		if (periodicResetDiff > PERIODIC_RESET_TIMEOUT) {
			LOG_PRINTF("Periodic reset!\n");
			delay(2000);
			ESP.restart();
		}

		delay(2000);
	}
}

void ServerTask(void *pvParameters __attribute__((unused)))
{
	g_ctx.m_server
		.buildHandler("/")
		.setDisableAuthOverride()
		.on(HTTP_GET, indexHandler);

	g_ctx.m_server
		.buildHandler("/rssi")
		.setDisableAuthOverride()
		.on(HTTP_GET, rssiHandler);

	g_ctx.m_server
		.buildHandler("/reboot")
		.setDisableAuthOverride()
		.on(HTTP_GET, rebootHandler);

	g_ctx.m_server
		.buildHandler("/:operation/:value")
		.setDisableAuthOverride()
		.on(HTTP_GET, configureHandler);

	g_ctx.m_server.clearBuilders();
	g_ctx.m_server.begin();

	while (1) {
		delay(100);
	}
}

void telnetConnected()
{
	SERIAL.println("Telnet connection established.");
}

void telnetDisconnected()
{
	SERIAL.println("Telnet connection closed.");
}

void setLedColor(uint32_t color, const bool &forceFullBrightness = false);

void setup()
{
	//
	// make sure wifi is initialized before calling anything else
	//

	WiFi.mode(WIFI_MODE_STA);
	WiFi.setSleep(false);

	#if BUILD_PICO_STAMP
	SerialAndTelnet.setWelcomeMsg((char *)"Alarm beeper/Bell signal generator (M5Stamp variant) by Embedded Softworks, s.r.o.\n\n");
	#else
	SerialAndTelnet.setWelcomeMsg((char *)"Alarm beeper/Bell signal generator (M5AtomLite variant) by Embedded Softworks, s.r.o.\n\n");
	#endif
	SerialAndTelnet.setCallbackOnConnect(telnetConnected);
	SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);

	SERIAL.begin(115200);
	SERIAL.flush();
	delay(50);

#if BUILD_PICO_STAMP
	LOG_PRINTF("M5Stamp initializing...OK\n");

	// RBG led
	FastLED.addLeds<SK6812, DATA_PIN, RGB>(g_ctx.m_leds, NUM_LEDS);
	delay(10);
#else
	// Init M5Atom
	M5.begin(false, true, true);
	delay(10);
#endif

	// make sure durations/notes structures match
	assert(NUM_RING_NOTES == NUM_RING_DURATIONS);
	assert(NUM_NOTES1 == NUM_DURATIONS1);
	assert(NUM_NOTES2 == NUM_DURATIONS2);

	// apply song speed
	for (int i = 0; i < NUM_RING_NOTES; i++)
	{
		ringDurations[i] = ringDurations[i] / SPEED;
	}

	for (int i = 0; i < NUM_NOTES1; i++)
	{
		noteDurations1[i] = noteDurations1[i] / SPEED;
	}

	for (int i = 0; i < NUM_NOTES2; i++)
	{
		noteDurations2[i] = noteDurations2[i] / SPEED;
	}

	// red LED
	setLedColor(COLOR_RED, true);

	// configure buzzer for output
 	pinMode(BUZZER_PIN, OUTPUT);

#if 0
	// wait until serial is available
	while (!Serial) {};
#endif

	// create semaphore for watchdog
	g_ctx.m_mutex = xSemaphoreCreateMutex();

	watchdogReset();
	g_ctx.m_periodicResetTs = millis();

	xTaskCreatePinnedToCore(
			WatchdogTask,		// pointer to thread function
			"WatchdogTask",		// thread name
			8192,				// This stack size can be checked & adjusted by reading the Stack Highwater
			NULL,				// Parameters for the task
			3,					// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			NULL,				// Task Handle
			ARDUINO_RUNNING_CORE);
}

//
// HTTP handlers
//

void indexHandler(RequestContext& request)
{
	const char body[] =
#if BUILD_PICO_STAMP
	"Alarm beeper/Bell signal generator (M5Stamp variant)<br>"
#else
	"Alarm beeper/Bell signal generator (M5AtomLite variant)<br>"
#endif
	"(c) 2022 Embedded Softworks, s.r.o. <br>"
	"<br>"
	"Click <a href=\"/led/100\">here</a> to set LED brightness to 100<br>"
	"Click <a href=\"/alarm/on\">here</a> to turn alarm on<br>"
	"Click <a href=\"/alarm/off\">here</a> to turn alarm off<br>"
	"Click <a href=\"/bell/on\">here</a> to turn bell on<br>"
	"Click <a href=\"/bell/off\">here</a> to turn bell off<br>"
	"Click <a href=\"/rssi\">here</a> to get RSSI<br><br>"
	"<br>"
	"Click <a href=\"/reboot\">here</a> to reboot the device<br>";

	request.response.sendRaw(200, "text/html", body);
}

void rssiHandler(RequestContext& request)
{
	// print the received signal strength:
	long rssi = WiFi.RSSI();
	LOG_PRINTF("signal strength (RSSI): %d dBm\n", rssi);

	request.response.json["rssi"] = rssi;

	// add time parameter
	uint32_t currTimeMs = millis();
	request.response.json["currTimeMs"] = currTimeMs;
	request.response.json["currTime"] = msToTimeStr(currTimeMs);
	request.response.json["timeToReset"] = msToTimeStr(timeToReset());
}

void rebootHandler(RequestContext& request)
{
	// add time parameter
	uint64_t currTimeMs = millis();
	request.response.json["currTimeMs"] = currTimeMs;
	request.response.json["currTime"] = msToTimeStr(currTimeMs);
	request.response.json["timeToReset"] = msToTimeStr(timeToReset());

	// schedule reboot
	watchdogScheduleReboot();
}

void configureHandler(RequestContext& request)
{
	const char *opParam = request.pathVariables.get("operation");
	const char *valParam = request.pathVariables.get("value");
	if (!opParam || !valParam) {
		LOG_PRINTF("Please use /<operation>/<value> path!\n");
		request.response.json["error"] = "invalid";
	} else {

		// what configuration operation was requested?
		String opParamStr(opParam);

		if (opParamStr == "led") {
			int value = atoi(valParam);
			LOG_PRINTF("LED value: %d\n", value);
			setLedBrightness(value);
			request.response.json["led"] = value;
		} else if (opParamStr == "alarm") {
			String calStr(valParam);
			if (calStr == "on") {
				LOG_PRINTF("Alarm is on\n");
				g_ctx.m_alarmOn = true;
			} else {
				LOG_PRINTF("Alarm is off\n");
				g_ctx.m_alarmOn = false;
			}
		} else if (opParamStr == "bell") {
			String calStr(valParam);
			if (calStr == "on") {
				LOG_PRINTF("Bell is on\n");
				g_ctx.m_bellOn = true;
			} else {
				LOG_PRINTF("Bell is off\n");
				g_ctx.m_bellOn = false;
			}
		}
	}

	// add time parameter
	uint32_t currTimeMs = millis();
	request.response.json["currTimeMs"] = currTimeMs;
	request.response.json["currTime"] = msToTimeStr(currTimeMs);
	request.response.json["timeToReset"] = msToTimeStr(timeToReset());
}

void recalculateLed(const bool &forceFullBrightness = false)
{
	uint8_t brightness = forceFullBrightness ? 0xff : g_ctx.m_brightness;
#if	BUILD_PICO_STAMP
	uint8_t b = ((uint8_t *)&g_ctx.m_color)[0];
	uint8_t r = ((uint8_t *)&g_ctx.m_color)[1];
	uint8_t g = ((uint8_t *)&g_ctx.m_color)[2];

	g_ctx.m_leds[0] = COLOR(r, g, b, g_ctx.m_ledOn ? brightness : 0);
	FastLED.show();
#else
	uint8_t r = ((uint8_t *)&g_ctx.m_color)[2];
	uint8_t g = ((uint8_t *)&g_ctx.m_color)[1];
	uint8_t b = ((uint8_t *)&g_ctx.m_color)[0];

	M5.dis.drawpix(0, COLOR(r, g, b, g_ctx.m_ledOn ? brightness : 0));
#endif
}

void setLedBrightness(uint8_t brightness)
{
	g_ctx.m_brightness = brightness;
	recalculateLed();
}

void setLedColor(uint32_t color, const bool &forceFullBrightness)
{
	g_ctx.m_color = color;
	recalculateLed(forceFullBrightness);
}

void advanceLedBlink()
{
	// tick every 100ms
	if (millis() - g_ctx.m_lastMillis > 100) {
		g_ctx.m_lastMillis = (millis() / 100) * 100;

		if (!g_ctx.m_ledCycle) {
			g_ctx.m_ledOn = true;
		} else {
			g_ctx.m_ledOn = false;
		}
		recalculateLed();
		g_ctx.m_ledCycle++; 

		// blink ever 5 seconds
		if (g_ctx.m_ledCycle > 50) {
			g_ctx.m_ledCycle = 0;
		}
	}
}

void setNote(int note)
{
	static int lastNote = -1;

	if (note != lastNote)
	{
		LOG_PRINTF("Setting note %d\n", note);
		lastNote = note;

		if (note < 0) {
			ledcDetachPin(BUZZER_PIN);
		} else {
			ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
			ledcWriteTone(BUZZER_PWM_CHANNEL, note);
		}
	}
}

void loop()
{
	//
	// We start by connecting to a WiFi network
	//

	if (!wifiReconnect()) {
		LOG_PRINTF("Failed to reconnect, restarting board!\n");
		setLedColor(COLOR_RED, true);
		ESP.restart();
	}

	//
	// arduino ota
	//

	ArduinoOTA
		.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			SERIAL.println("Start updating " + type);
		})
		.onEnd([](){
			SERIAL.println("\nEnd");
		})
		.onProgress([](unsigned int progress, unsigned int total) {
			SERIAL.printf("Progress: %u%%\r", (progress / (total / 100)));
			watchdogReset();
		})
		.onError([](ota_error_t error) {
			SERIAL.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) SERIAL.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) SERIAL.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) SERIAL.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) SERIAL.println("Receive Failed");
			else if (error == OTA_END_ERROR) SERIAL.println("End Failed");
		});

	ArduinoOTA.begin();

	// green LED - we are ready to process clients
	setLedColor(COLOR_GREEN);

	// process HTTP clients on background thread
	xTaskCreatePinnedToCore(
			ServerTask,			// pointer to thread function
			"ServerTask",		// thread name
			32768,				// This stack size can be checked & adjusted by reading the Stack Highwater
			NULL,				// Parameters for the task
			2,					// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			NULL,				// Task Handle
			ARDUINO_RUNNING_CORE);

	// disable beep
	setNote(-1);

	while (1) {

		// handle OTA support
		ArduinoOTA.handle();

		// handle telnet
		SerialAndTelnet.handle();

		// update watchdog time
		watchdogReset();

		// periodic Wifi check
		if (!wifiCheckConnection()) {

			// red color indicates network problem
			setLedColor(COLOR_RED, true);

			if (!wifiReconnect()) {
				LOG_PRINTF("Failed to reconnect, restarting board!\n");
				ESP.restart();
			} else {
				// we have reconnected successfully
				setLedColor(COLOR_GREEN);
			}
		}

		advanceLedBlink();
		processTone();
	}
}

void processTone()
{
	//
	// are bell / alarm buttons pressed?
	//

	bool bellPressed = false;
	bool alarmPressed = false;

	// check if the bell is active
	g_bellButton.read();
	if (g_bellButton.isPressed()) {
		bellPressed = true;
	} else {
		bellPressed = false;
	}

	alarmPressed |= g_ctx.m_alarmOn;
	bellPressed |= g_ctx.m_bellOn;

	static bool prevBellPressed = false;
	static bool prevAlarmPressed = false;

	if (bellPressed != prevBellPressed)
	{
		prevBellPressed = bellPressed;
		LOG_PRINTF("Bell button %s\n", bellPressed ? "pressed" : "released");
	}

	if (alarmPressed != prevAlarmPressed)
	{
		prevAlarmPressed = alarmPressed;
		LOG_PRINTF("Alarm button %s\n", alarmPressed ? "pressed" : "released");
	}

	//
	// is the alarm button pressed?
	//

	if (alarmPressed)
	{
		alarmButtonPressed();
	}

	//
	// is the bell button pressed?
	//

	else if (bellPressed)
	{
		bellButtonPressed();
	}

	//
	// no melody running, clear note
	//

	else
	{
		g_ctx.bellRunning = false;
		g_ctx.alarmRunning = false;
		setNote(-1);
	}
}

void bellButtonPressed()
{
	// shall we start a new melody?
	if (!g_ctx.bellRunning)
	{

		//
		// new melody sequence initialization
		//

		g_ctx.bellRunning = true;
		g_ctx.melodyStart = millis();
		g_ctx.notePos = 0;
		g_ctx.melodyPos = 0;
		g_ctx.repeatCnt = 0;
	}

repeat:
	//
	// get the current melody
	//

	int *melody = ringMelody;
	int *noteDurations = ringDurations;
	int numNotes = NUM_RING_NOTES;

	// have we played everything?
	if ((g_ctx.notePos >= numNotes) || (g_ctx.repeatCnt >= NUM_RING_REPEATS))
	{
		// final state - muted
		setNote(-1);
		return;
	}

	// get current relative time
	unsigned int timeSinceStart = millis() - g_ctx.melodyStart;

	// switch to a next note if needed
	if (timeSinceStart > noteDurations[g_ctx.notePos])
	{

		// switch to the next note
		g_ctx.notePos++;
		g_ctx.melodyStart = millis();

		// have we played all notes? then jump to the next sequence
		if (g_ctx.notePos >= numNotes)
		{

			// increment repeat counter
			g_ctx.repeatCnt++;

			// start from first note again
			g_ctx.notePos = 0;

			// if we run through all repeats, leave
			if (g_ctx.repeatCnt >= NUM_RING_REPEATS)
			{
				goto repeat;
			}
		}
	}

	// set the tone for this note
	setNote(melody[g_ctx.notePos]);
}

void alarmButtonPressed()
{
	// shall we start a new melody?
	if (!g_ctx.alarmRunning)
	{

		//
		// new melody sequence initialization
		//

		g_ctx.alarmRunning = true;
		g_ctx.melodyStart = millis();
		g_ctx.notePos = 0;
		g_ctx.melodyPos = 0;
		g_ctx.repeatCnt = 0;
	}

repeat:
	//
	// get the current melody
	//

	int *melody = NULL;
	int *noteDurations = NULL;
	int numNotes = 0;

	if (g_ctx.melodyPos == 0)
	{
		melody = melody1;
		noteDurations = noteDurations1;
		numNotes = NUM_NOTES1;
	}
	else
	{
		melody = melody2;
		noteDurations = noteDurations2;
		numNotes = NUM_NOTES2;
	}

	// have we played everything?
	if ((g_ctx.notePos >= numNotes) || (g_ctx.repeatCnt >= NUM_REPEATS) || (g_ctx.melodyPos >= NUM_MELODIES))
	{
		// final state - beeping forever
		setNote(BEEP_NOTE);
		return;
	}

	// get current relative time
	unsigned int timeSinceStart = millis() - g_ctx.melodyStart;

	// switch to a next note if needed
	if (timeSinceStart > noteDurations[g_ctx.notePos])
	{

		// switch to the next note
		g_ctx.notePos++;
		g_ctx.melodyStart = millis();

		// have we played all notes? then jump to the next sequence
		if (g_ctx.notePos >= numNotes)
		{

			// increment repeat counter
			g_ctx.repeatCnt++;

			// start from first note again
			g_ctx.notePos = 0;

			// if we run through all repeats, leave
			if (g_ctx.repeatCnt >= NUM_REPEATS)
			{

				// jump to the next melody
				g_ctx.melodyPos++;
				g_ctx.repeatCnt = 0;
				goto repeat;
			}
		}
	}

	// set the tone for this note
	setNote(melody[g_ctx.notePos]);
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
