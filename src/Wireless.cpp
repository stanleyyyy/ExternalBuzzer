#include "Wireless.h"
#include <WiFi.h>

#include "definitions.h"
#include "utilities.h"

//
// Wifi access credentials
//

const char ssid[] = SECRET_SSID; // your network SSID (name)
const char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
const int keyIndex = 0;		   // your network key Index number (needed only for WEP)

wl_status_t g_prevWirelessStatus = WL_NO_SHIELD;

bool wifiCheckConnection()
{
	wl_status_t status = (wl_status_t)WiFi.status();
	if (status != g_prevWirelessStatus)
	{
		g_prevWirelessStatus = status;

		// by default any state change means a failure
		bool success = false;

		switch (status)
		{
		case WL_NO_SHIELD:
			LOG_PRINTF("Wifi state changed to 'No Module'\n");
			break;

		case WL_IDLE_STATUS:
			LOG_PRINTF("Wifi state changed to 'Idle'\n");
			break;

		case WL_NO_SSID_AVAIL:
			LOG_PRINTF("Wifi state changed to 'No SSID available'\n");
			break;

		case WL_SCAN_COMPLETED:
			LOG_PRINTF("Wifi state changed to 'Scan completed'\n");
			break;

		case WL_CONNECTED:
			LOG_PRINTF("Wifi state changed to 'Connected'\n");
			success = true;
			break;

		case WL_CONNECT_FAILED:
			LOG_PRINTF("Wifi state changed to 'Connection failed'\n");
			break;

		case WL_CONNECTION_LOST:
			LOG_PRINTF("Wifi state changed to 'Connection lost'\n");
			break;

		case WL_DISCONNECTED:
			LOG_PRINTF("Wifi state changed to 'Disconnected'\n");
			break;

		default:
			LOG_PRINTF("Wifi state changed to 'Unknown'\n");
			break;
		}

		return success;
	}

	return true;
}

bool wifiReconnect()
{
	LOG_PRINTF("Connecting to %s\n", ssid);

	WiFi.begin(ssid, pass);

	int cnt = 20;

	while (cnt--)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			LOG_PRINTF("WiFi connected\n");
			LOG_PRINTF("IP address: %s\n", WiFi.localIP().toString().c_str());
			return true;
		}
		watchdogReset();
		delay(500);
		Serial.print(".");
	}

	return false;
}

