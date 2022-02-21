#pragma once

//
// debug uart speed
//

#define HW_UART_SPEED 115200L

//
// 5 second timeout to reset the board
//

#define WATCHDOG_TIMEOUT 5000

//
// Timeout for the WiFi connection. When this is reached,
// the ESP goes into deep sleep for 30seconds to try and
// recover.
//

#define WIFI_TIMEOUT 10000 // 10 seconds

// The maximum number of connection retries
#define WIFI_RETRIES 3

//
// Syncing time with an NTP server
//

#define NTP_TIME_SYNC_ENABLED true
#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET_SECONDS 3600
#define NTP_UPDATE_INTERVAL_MS (5 * 60 * 1000)

//
// Wifi related settings
//

#define ESPASYNC_WIFIMGR_DEBUG_PORT SERIAL
#define _ESPASYNC_WIFIMGR_LOGLEVEL_ 2		// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define DOUBLERESETDETECTOR_DEBUG true		// double reset detector enabled
#define DRD_TIMEOUT 10						// Number of seconds after reset during which a subseqent reset will be considered a double reset.
#define DRD_ADDRESS 0						// RTC Memory Address for the DoubleResetDetector to use
#define HEARTBEAT_INTERVAL 10000
#define MIN_AP_PASSWORD_SIZE 8
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64
#define WIFICHECK_INTERVAL 1000L
#define CONFIG_FILENAME F("/wifi_cred.dat")
#define LAST_PARAMS_FILENAME F("/wifi_last_params.dat")
#define USING_CORS_FEATURE false
#define USE_DHCP_IP true
#define USE_CONFIGURABLE_DNS true
#define USE_CUSTOM_AP_IP false
#define ESP_DRD_USE_SPIFFS true
#define HOST_NAME_LEN 40
#define HTTP_PORT 80
#define RESET_WHEN_RECONFIGURING_WIFI 0		// set to 1 to reconfigure wifi via esp32 reset
#define PRINT_PASSWORDS 0					// set to 1 to show plaintext passwords in console

#if ESP32
	// For ESP32, this better be 0 to shorten the connect time.
	// For ESP32-S2/C3, must be > 500
	#if (USING_ESP32_S2 || USING_ESP32_C3)
		#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 500L
	#else
		// For ESP32 core v1.0.6, must be >= 500
		#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 800L
	#endif
#else
	// For ESP8266, this better be 2200 to enable connect the 1st time
	#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 2200L
#endif

#define WIFI_MULTI_CONNECT_WAITING_MS 500L

//
// Periodic reset every 24 hours
//

#define PERIODIC_RESET_TIMEOUT (24 * 60 * 60 * 1000)

//
// color macros
//

#define COLOR(r, g, b, a) (((((int)r * a / 256) & 0xFF) << 16) | ((((int)g * a / 256) & 0xFF) << 8) | ((((int)b * a / 256) & 0xFF) << 0))

#define BRIGHTNESS 0

#define COLOR_RED 		0x00FF0000
#define COLOR_GREEN		0x0000FF00
#define COLOR_YELLOW	0x00FFF000

//
// in/out pins
//

#if BUILD_PICO_STAMP
// How many leds in your strip?
#define NUM_LEDS 1
#define DATA_PIN 27
#endif

#define BUZZER_PIN 21		// G21
#define INPUT_BELL_PIN 22	// G22
#define BUZZER_PWM_CHANNEL 0

// RFID card reader signal duration
#define RFID_DURATION_MS 300

// 60 seconds to repeat each sequence
#define NUM_REPEATS 200

// beep note
#define BEEP_NOTE NOTE_C7

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

