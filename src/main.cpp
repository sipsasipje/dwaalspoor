#include <Arduino.h>
#include "RTC.h"
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "secrets.h"
#include <Adafruit_NeoPixel.h>

// Settings
int hold = 5000;
int fade = 1000;

// LED strip configuration.
#define LEDS_PIN 2
#define LEDS_NUM 20
Adafruit_NeoPixel LEDS = Adafruit_NeoPixel(LEDS_NUM, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// UV LED configuration.
#define UV_PIN 3

// Program state.
#define duration millis() - stamp
unsigned long stamp = 0; // Timestamp

uint32_t currentColor;
int nextRGBColor = 0; // Set color to: 0 - R, 1 - G, 2 - B

enum State
{
  setColor,
  fadeIn,
  holdColor,
  fadeOut,
  uvOn,
  uvOff
};
State state = State::setColor;

/**
 * Gets next color in RGB sequence.
 *
 * @return uint32_t - RGB color.
 */
uint32_t getNextRGBColor()
{
  int rgb[3] = {0, 0, 0};
  rgb[nextRGBColor] = 255;

  nextRGBColor = (nextRGBColor + 1) % 3;

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

/**
 * Get specific color or random color if not specified.
 *
 * @param int *rgb - RGB color values 0-255.
 */
uint32_t getColor(int *rgb = nullptr)
{
  if (rgb == nullptr)
  {
    for (int i = 0; i <= 2; i++)
    {
      rgb[i] = random(0, 255);
    }
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

/**
 * Calculate the ratio of the fade at the current time.
 *
 * @param bool out - Fade out.
 * @return uint8_t - Ratio.
 */
float getFadeRatio(bool out = false)
{
  float ratio = float(duration) / float(fade);
  return !out ? ratio : 1.0 - ratio;
}

/**
 * Get a color in a different brightness.
 *
 * @return uint32_t - Adjusted color.
 */
uint32_t getBrightnessColor(uint32_t color, float ratio)
{
  ratio = constrain(ratio, 0.0f, 1.0f); // Brightness ratio.

  int rgb[3];
  int bytes[3] = {16, 8, 0}; // R, G, B bytes.

  for (int i; i <= 2; i++)
  {
    int byte = (color >> bytes[i]) & 0xFF; // Separate the R, G, B bytes
    rgb[i] = (uint8_t)(byte * ratio);      // Set the brightness of each value.
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

/**
 * Setup the Arduino.
 */
void setup()
{
  Serial.begin(9600); // Enable writing messages to monitor.
  while (!Serial)

    pinMode(LEDS_PIN, OUTPUT);
  pinMode(UV_PIN, OUTPUT);

  LEDS.begin();
}

/**
 * Main loop.
 */
void loop()
{
  switch (state)
  {
  case setColor:
    currentColor = getNextRGBColor();
    LEDS.fill(currentColor, 0, LEDS_NUM);

    stamp = millis();
    state = State::fadeIn;
    break;
  case fadeIn:
  case fadeOut:
  {
    bool out = state == fadeOut ? true : false;
    uint32_t newColor = getBrightnessColor(currentColor, getFadeRatio(out));

    LEDS.fill(newColor, 0, LEDS_NUM);
    LEDS.show();

    if (duration >= fade)
    {
      stamp = millis();
      state = !out ? State::holdColor : State::setColor;
    }
    break;
  }
  case holdColor:
    if (duration >= hold)
    {
      stamp = millis();
      state = State::fadeOut;
    }
    break;
  }
}
