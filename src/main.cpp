#include <Arduino.h>
#include "RTC.h"
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "secrets.h"
#include <Adafruit_NeoPixel.h>

#define LEDS_PIN 2
#define LEDS_NUM 20

Adafruit_NeoPixel LEDS = Adafruit_NeoPixel(LEDS_NUM, LEDS_PIN, NEO_GRB + NEO_KHZ800);

#define UV_PIN 3

int nextRGBColor = 0;   // Set color to: 0 - R, 1 - G, 2 - B
unsigned long then = 0; // Time of last update.
int interval = 5000;

// Gets next color in RGB sequence.
uint32_t getNextRGBColor()
{
  int rgb[3] = {0, 0, 0};
  rgb[nextRGBColor] = 255;

  nextRGBColor = (nextRGBColor + 1) % 3;

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

// Gets specific color or random color if not specified.
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

// Changes color of LEDs.
void changeLEDSColor(uint32_t color)
{
  unsigned long now = millis();

  if (now - then >= interval)
  {
    LEDS.fill(color, 0, LEDS_NUM);
  }
}

void fadeIn()
{
  // float brightness = 255.0 * (float(millis() - fadeStart) / 1000);
  LEDS.setBrightness(0);
}

// Updates program state.
void state()
{
  changeLEDSColor(getNextRGBColor());
}

// Update the display.
void display()
{
  LEDS.show();
}

void setup()
{
  Serial.begin(9600); // Enable writing messages to monitor.

  pinMode(LEDS_PIN, OUTPUT);
  pinMode(UV_PIN, OUTPUT);

  LEDS.begin();

  state();
  display();
}

void loop()
{
  unsigned long now = millis();

  if (now - then >= interval)
  {
    then = now;

    state();
    display();
  }
}
