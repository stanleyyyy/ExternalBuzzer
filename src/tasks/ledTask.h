#pragma once

void setLedBrightness(uint8_t brightness);
void setLedColor(uint32_t color, const bool &forceFullBrightness = false);

void ledTask(void *pvParameters __attribute__((unused)));

