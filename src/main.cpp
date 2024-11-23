#include <Arduino.h>
#include "RTC.h"
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "secrets.h"
#include <Adafruit_NeoPixel.h>

enum State
{
  setColor,
  fadeIn,
  holdColor,
  fadeOut,
  xFade,
  uvOn,
  uvOff
};

struct Sequence
{
  State *states;
  uint8_t length;
  uint8_t repeat;
  bool removeWhenDone;
};

/**
 * Settings
 */
#define LEDS_PIN 4
#define LEDS_NUM 300

unsigned int hold = 5000; // Hold the lights.
unsigned int fade = 500;  // Fade duration.

// Sequences of steps we want to execute.
State sequence1[] = {setColor, fadeIn, holdColor, fadeOut};
State sequence2[] = {uvOn, uvOff};

// In what order, length of the sequence and how many times we want to execute the sequences.
Sequence sequences[] = {
    {sequence1, 4, 3, false},
    {sequence2, 2, 1, true}};
/**
 * End settings
 */

// LED strip configuration.
Adafruit_NeoPixel LEDS = Adafruit_NeoPixel(LEDS_NUM, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// UV LED configuration.
#define UV_PIN 3

// Program state.
#define duration millis() - stamp
unsigned long stamp = 0; // Timestamp

State state;
uint32_t currentColor;
uint8_t rgbIndex = 0; // Set color to: 0 - R, 1 - G, 2 - B
bool isHolding = false;

void setState()
{
  static uint8_t sequenceIndex = 0, stateIndex = 0, repeatIndex = 0;

  uint8_t numSequences = sizeof(sequences) / sizeof(Sequence);
  Sequence sequence = sequences[sequenceIndex];
  uint8_t sequenceLength = sequence.length;
  uint8_t sequenceRepeat = sequence.repeat;
  bool removeWhenDone = sequence.removeWhenDone;

  state = sequence.states[stateIndex];

  stateIndex++;

  if (stateIndex >= sequenceLength)
  { // We've reached the end of the current sequence.
    stateIndex = 0;
    repeatIndex++;

    if (repeatIndex >= sequenceRepeat)
    { // We've repeated the sequence the desired number of times.
      repeatIndex = 0;
      sequenceIndex++; 

      if (sequenceIndex >= numSequences)
      { // We've reached the end of all sequences.
        sequenceIndex = 0;
      }
    }
  }
}

/**
 * Get specific color or random color if not specified.
 *
 * @param uint8_t *rgb - RGB color values 0-255.
 */
uint32_t getColor(uint8_t *rgb = nullptr)
{
  if (rgb == nullptr)
  {
    for (uint8_t i = 0; i <= 2; i++)
    {
      rgb[i] = random(0, 255);
    }
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

/**
 * Gets color in RGB sequence.
 *
 * @param uint8_t byte - Byte to set to 255.
 *
 * @return uint32_t - Packed RGB color.
 */
uint32_t getRGBColor(uint8_t byte = 0)
{
  uint8_t rgb[3] = {0, 0, 0};
  rgb[byte] = 255;

  return getColor(rgb);
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

  uint8_t rgb[3];                // Resulting color.
  uint8_t bytes[3] = {16, 8, 0}; // R, G, B bytes.

  for (uint8_t i = 0; i <= 2; i++)
  {
    uint8_t byte = (color >> bytes[i]) & 0xFF; // Separate the R, G, B bytes of the input color.
    rgb[i] = (uint8_t)(byte * ratio);          // Set the brightness of each separate value.
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

uint32_t fadeColor(uint32_t startColor, uint32_t endColor, float ratio)
{
  ratio = constrain(ratio, 0.0f, 1.0f); // Ensure the ratio is between 0 and 1.

  uint8_t start[3];
  uint8_t end[3];
  uint8_t rgb[3];
  uint8_t bytes[3] = {16, 8, 0}; // R, G, B bytes.

  // Extract R, G, B values for start and end colors
  for (uint8_t i = 0; i <= 2; i++)
  {
    start[i] = (startColor >> bytes[i]) & 0xFF;
    end[i] = (endColor >> bytes[i]) & 0xFF;

    // Interpolate each color channel based on the ratio
    rgb[i] = (uint8_t)((1.0f - ratio) * start[i] + ratio * end[i]);
  }

  // Return the new color by combining the R, G, B components
  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

/**
 * Initialize the next state.
 */
void next()
{
  stamp = millis();
  setState();
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

  setState();
}

/**
 * Main loop.
 */
void loop()
{
  switch (state)
  {
  case setColor:
    currentColor = getRGBColor(rgbIndex);
    rgbIndex = (rgbIndex + 1) % 3;

    LEDS.fill(currentColor, 0, LEDS_NUM);

    next();
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
      next();
    }
    break;
  }
  case xFade:
  {
    uint32_t newColor = fadeColor(currentColor, getRGBColor(rgbIndex), getFadeRatio());
    
    LEDS.fill(newColor, 0, LEDS_NUM);
    LEDS.show();

    if (duration >= fade)
    {
      next();
    }
    break;
  }
  case holdColor:
    if (!isHolding){
      LEDS.show();
      isHolding = true; 
    }

    if (duration >= hold)
    {
      isHolding = false;
      next();
    }
    break;
  case uvOn:
    digitalWrite(UV_PIN, HIGH);

    next();
    break;
  case uvOff:
    if (duration >= hold)
    {
      digitalWrite(UV_PIN, LOW);

      next();
    }
    break;
  }

  delay(10);
}