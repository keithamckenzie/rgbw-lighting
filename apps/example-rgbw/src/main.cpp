#include <Arduino.h>
#include "rgbw.h"
#include "led_strip.h"

#define LED_PIN     5
#define NUM_LEDS    30

LEDStrip<StripType::SK6812_RGBW> strip(LED_PIN, NUM_LEDS);

void setup() {
    Serial.begin(115200);
    strip.begin();

    // Set all LEDs to warm white
    strip.fill(RGBW(0, 0, 0, 128));
    strip.show();
}

void loop() {
    // Cycle through hues
    static uint16_t hue = 0;
    HSV hsv = { hue, 255, 200 };
    RGBW color = hsvToRGBW(hsv);

    strip.fill(color);
    strip.show();

    hue = (hue + 1) % 360;
    delay(20);
}
