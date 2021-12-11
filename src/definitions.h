#pragma once

#define SECRET_SSID	"EmbeddedUnifi" // your network SSID (name)
#define SECRET_PASS	"Arthur1986"	// your network password (use for WPA, or use as key for WEP)

//
// debug uart speed
//

#define HW_UART_SPEED 115200L

//
// 5 second timeout to reset the board
//

#define WATCHDOG_TIMEOUT 5000

//
// Periodic reset every 8 hours
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

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

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
