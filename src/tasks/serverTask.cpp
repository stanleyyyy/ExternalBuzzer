#include <Arduino.h>

#include <ESPAsyncWebserver.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

#include "WiFi.h"
#include "config.h"
#include "utils.h"
#include "watchdog.h"

#include "wifiTask.h"
#include "serverTask.h"
#include "ledTask.h"
#include "ntpTask.h"
#include "beeperTask.h"

#define OUTPUT_JSON_BUFFER_SIZE 512

#if BUILD_PICO_STAMP
#define TITLE "Alarm beeper/Bell signal generator (M5Stamp variant)<br>"
#else
#define TITLE "Alarm beeper/Bell signal generator (M5AtomLite variant)<br>"
#endif


class ServerTaskCtx {
private:
	bool m_wifiReconfigureRequested;
	bool m_wifiResetRequested;

public:
	ServerTaskCtx()
	{
		m_wifiReconfigureRequested = false;
		m_wifiResetRequested = false;
	}

	//
	// HTTP handlers
	//

	void indexHandler(AsyncWebServerRequest *request)
	{
		LOG_PRINTF("%s(%d): request from %s\n", __FUNCTION__, __LINE__, request->client()->remoteIP().toString().c_str());
		String body =
		"<!DOCTYPE html>\n"
		"<html>\n"
		"<title>" TITLE "</title>\n"
		"<meta charset=\"UTF-8\">\n"
		"<meta http-equiv=\"refresh\" content=\"5\">\n"
		"<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
		"<style>\n"
		"html { font-family: Helvetica; display: inline-block; margin: 10px auto}\n"
		"body{margin-top: 0px;} h1 {color: #444444;margin: 50px auto 30px;}\n"
		"p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n"
		"</style>\n"
		"</head>\n"
		"<body>\n"
		"<h2>" TITLE "</h2>\n"
		"(c) 2021 Embedded Softworks, s.r.o. <br>";

		body +=
		"Click <a href=\"/led/?value=100\">here</a> to set LED brightness to 100<br>"
		"Click <a href=\"/alarm?value=on\">here</a> to turn alarm on<br>"
		"Click <a href=\"/alarm?value=off\">here</a> to turn alarm off<br>"
		"Click <a href=\"/bell?value=on\">here</a> to turn bell on<br>"
		"Click <a href=\"/bell?value=off\">here</a> to turn bell off<br>"
		"Click <a href=\"/rssi\">here</a> to get RSSI<br><br>";

		body +=
		"<br>"
		"Click <a href=\"/reconfigureWifi\">here</a> to reconfigure Wifi<br><br>"
		"Click <a href=\"/resetWifi\">here</a> to erase all Wifi settings<br>"
		"Click <a href=\"/reboot\">here</a> to reboot the device<br>"
		"</body>"
		"</html>";

		request->send(200, "text/html", body.c_str());
	}

	void rssiHandler(AsyncWebServerRequest *request)
	{
		LOG_PRINTF("%s(%d): request from %s\n", __FUNCTION__, __LINE__, request->client()->remoteIP().toString().c_str());
		StaticJsonDocument<OUTPUT_JSON_BUFFER_SIZE> doc;

		// print the received signal strength:
		long rssi = WiFi.RSSI();
		LOG_PRINTF("signal strength (RSSI): %d dBm\n", rssi);

		doc["rssi"] = rssi;

		// add time parameter
		uint64_t currTimeMs = compensatedMillis();
		doc["currTimeMs"] = currTimeMs;
		doc["currTime"] = msToTimeStr(currTimeMs);
		doc["watchdogTimeToReset"] = msToTimeStr(watchdogTimeToReset());

		char buffer[OUTPUT_JSON_BUFFER_SIZE];
		serializeJson(doc, buffer, sizeof(buffer));
		request->send(200, "application/json", buffer);
	}

	void ledHandler(AsyncWebServerRequest *request)
	{
		LOG_PRINTF("%s(%d): request from %s\n", __FUNCTION__, __LINE__, request->client()->remoteIP().toString().c_str());

		if (request->hasParam("value")) {
			int value = atoi(request->getParam("value")->value().c_str());
			LOG_PRINTF("LED value: %d\n", value);
			setLedBrightness(value);
			request->redirect("/index");
		} else {
			request->send(404, "text/plain", "Not found");
		}
	}

	void alarmHandler(AsyncWebServerRequest *request)
	{
		LOG_PRINTF("%s(%d): request from %s\n", __FUNCTION__, __LINE__, request->client()->remoteIP().toString().c_str());

		int duration = 0;

		if (request->hasParam("duration")) {
			duration = atoi(request->getParam("duration")->value().c_str());

			// limit duration to <0;1000>
			if (duration < 0)
				duration = 0;

			if (duration > 1000)
				duration = 1000;
		}

		if (request->hasParam("value")) {
			if (request->getParam("value")->value() == "on") {
				LOG_PRINTF("Alarm is on\n");
				beeperAlarmOn(true);
				if (duration) {
					delay(duration);
					beeperAlarmOn(false);
				}
			} else {
				LOG_PRINTF("Alarm is off\n");
				beeperAlarmOn(false);
				if (duration) {
					delay(duration);
					beeperAlarmOn(true);
				}
			}
			request->redirect("/index");
		} else {
			request->send(404, "text/plain", "Not found");
		}
	}

	void bellHandler(AsyncWebServerRequest *request)
	{
		LOG_PRINTF("%s(%d): request from %s\n", __FUNCTION__, __LINE__, request->client()->remoteIP().toString().c_str());

		if (request->hasParam("value")) {
			if (request->getParam("value")->value() == "on") {
				LOG_PRINTF("Bell is on\n");
				beeperBellOn(true);
			} else {
				LOG_PRINTF("Bell is off\n");
				beeperBellOn(false);
			}
			request->redirect("/index");
		} else {
			request->send(404, "text/plain", "Not found");
		}
	}

	void reconfigureWifiHandler(AsyncWebServerRequest *request)
	{
		String body =
		"<!DOCTYPE html>"
		"<html>"
		"<meta http-equiv=\"refresh\" content=\"2; url=/index\">"
		"<body>"
		"WiFi reconfiguration initiated"
		"</body>";
		request->send(200, "text/html", body);
		m_wifiReconfigureRequested = true;
	}

	void resetWifi(AsyncWebServerRequest *request)
	{
		String body =
		"<!DOCTYPE html>"
		"<html>"
		"<meta http-equiv=\"refresh\" content=\"2; url=/index\">"
		"<body>"
		"Resetting WiFi..."
		"</body>";
		request->send(200, "text/html", body);

		// give it enough time to deliver response back to caller
		delay(100);

		// now reset
		m_wifiResetRequested = true;
	}

	void rebootHandler(AsyncWebServerRequest *request)
	{
		String body =
		"<!DOCTYPE html>"
		"<html>"
		"<meta http-equiv=\"refresh\" content=\"10; url=/index\">"
		"<body>"
		"Rebooting..."
		"</body>";
		request->send(200, "text/html", body);

		// schedule reboot
		watchdogScheduleReboot();
	}

	void init()
	{		
		// green LED - we are ready to process clients
		setLedColor(COLOR_GREEN);
	}

	void task()
	{
		AsyncWebServer *server = wifiGetHttpServer();
		bool shallInitServer = true;

		// first initialization
		init();

		while (1) {

			//
			// init server handlers if necessary
			//

			if (shallInitServer) {
				shallInitServer = false;

				server->on("/", HTTP_GET, [=](AsyncWebServerRequest *request){
					indexHandler(request);
				});

				server->on("/index", HTTP_GET, [=](AsyncWebServerRequest *request){
					indexHandler(request);
				});

				server->on("/rssi", HTTP_GET, [=](AsyncWebServerRequest *request){
					rssiHandler(request);
				});

				server->on("/led", HTTP_GET, [=](AsyncWebServerRequest *request){
					ledHandler(request);
				});

				server->on("/alarm", HTTP_GET, [=](AsyncWebServerRequest *request){
					alarmHandler(request);
				});

				server->on("/bell", HTTP_GET, [=](AsyncWebServerRequest *request){
					bellHandler(request);
				});

				server->on("/reconfigureWifi", HTTP_GET, [=](AsyncWebServerRequest *request){
					reconfigureWifiHandler(request);
				});

				server->on("/resetWifi", HTTP_GET, [=](AsyncWebServerRequest *request){
					resetWifi(request);
				});

				server->on("/reboot", HTTP_GET, [=](AsyncWebServerRequest *request){
					rebootHandler(request);
				});

				server->onNotFound([=](AsyncWebServerRequest *request){
					request->send(404, "text/plain", "Not found");
				});

				server->begin();

				if (!MDNS.begin(wifiHostName().c_str())) {
					LOG_PRINTF("Error starting MDNS responder!\n");
				}

				// Add service to MDNS-SD so our webserver can be located
				MDNS.addService("http", "tcp", 80);
			}

			//
			// handle wifi reconfigure request
			//

			if (m_wifiReconfigureRequested) {
				m_wifiReconfigureRequested = false;
				LOG_PRINTF("WiFi reconfiguration requested\n");

				// reset server handlers
				server->reset();

				// init wifi reconfiguration
				wifiReconfigure();

				// and now we have to re-init the server again
				shallInitServer = true;
			}

			//
			// handle wifi reset request
			//

			if (m_wifiResetRequested) {
				m_wifiResetRequested = false;
				LOG_PRINTF("WiFi reset requested\n");

				// reset server handlers
				server->reset();

				// init wifi reset
				wifiReset();

				// and now we have to re-init the server again
				shallInitServer = true;
			}

			delay(100);
		}
	}

};

static ServerTaskCtx g_ctx;

void serverTask(void *pvParameters __attribute__((unused)))
{
	g_ctx.task();
}
