#include "Arduino.h"

#include <stdarg.h>
#include "utilities.h"
// #include "snprintf.h"
#include "definitions.h"

#define LOG_SIZE_MAX 2048

void printf_internal(const char *fmt, ...)
{
	char buf[LOG_SIZE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, LOG_SIZE_MAX, fmt, ap);
	va_end(ap);

	SERIAL.print(msToTimeStr(millis()));
	SERIAL.print(": ");
	SERIAL.print(buf);
}

char *msToTimeStr(unsigned long ms)
{
	static char timeBuf[32];

	unsigned long h = ms / 3600000;
	ms = ms - h * 3600000;

	unsigned long m = ms / 60000;
	ms = ms - m * 60000;

	unsigned long s = ms / 1000;
	ms = ms - s * 1000;

	snprintf(timeBuf, sizeof(timeBuf) - 1, "%02lu:%02lu:%02lu.%03lu", h, m, s, ms);
	return timeBuf;
}
