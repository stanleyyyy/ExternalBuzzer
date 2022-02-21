#if BUILD_PICO_STAMP
#include "Arduino.h"
#include <FastLED.h>
#else
#include "M5Atom.h"
#endif

#include "config.h"
#include "ledTask.h"
#include "utils.h"

class Context {
public:

	bool m_ledOn = false;
	uint8_t m_brightness = 0;
	uint32_t m_color = COLOR_RED;

#if BUILD_PICO_STAMP
	// Define the array of leds
	CRGB m_leds[NUM_LEDS];
#endif

	int32_t m_ledCycle = 0;
	uint32_t m_lastMillis = 0;

	Context()
	: m_ledOn(true)
	, m_brightness(BRIGHTNESS)
	, m_color(COLOR_RED)
	, m_ledCycle(0)
	, m_lastMillis(0)
	{
	}

};

static Context g_ctx;

static void recalculateLed(const bool &forceFullBrightness = false)
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

void ledTask(void *pvParameters __attribute__((unused)))
{
#if	BUILD_PICO_STAMP
	LOG_PRINTF("M5Stamp initializing...OK\n");

	// RBG led
	FastLED.addLeds<SK6812, DATA_PIN, RGB>(g_ctx.m_leds, NUM_LEDS);
	delay(10);
#endif

	while (1) {
#if 0
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
#endif
		advanceLedBlink();
	}
}