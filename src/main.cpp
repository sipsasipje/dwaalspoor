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

unsigned int hold = 2000; // Hold the lights.
unsigned int fade = 500;  // Fade duration.

// Sequences of steps we want to execute.
// State sequence1[] = {setColor, fadeIn, holdColor, fadeOut};
// State sequence2[] = {uvOn, uvOff};
State sequence1[] = {setColor, fadeIn, holdColor, xFade};
State sequence2[] = {setColor, holdColor, xFade};

// In what order, length of the sequence and how many times we want to execute the sequences.
// Sequence sequences[] = {
//     {sequence1, 4, 3, false},
//     {sequence2, 2, 2, true}};
Sequence sequences[] = {
    {sequence1, 4, 1, true},
    {sequence2, 3, 1, false}};
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
uint32_t nextColor;
uint8_t rgbIndex = 0; // Set color to: 0 - R, 1 - G, 2 - B
bool isHolding = false;

/**
 * Updates the current sequence and state.
 */
void setState()
{
  static uint8_t sequenceIndex = 0, stateIndex = 0, repeatIndex = 0;
  static uint8_t numSequences = sizeof(sequences) / sizeof(Sequence);

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

      if (removeWhenDone)
      {
        for (uint8_t i = sequenceIndex; i < numSequences; i++)
        {
          sequences[i - 1] = sequences[i]; // Shift the sequences left.
        }
        numSequences--; // We have one less sequence now.
      }

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
float getFadeRatio()
{
  bool isInverted = state == fadeOut;

  float ratio = float(duration) / float(fade);
  return !isInverted ? ratio : 1.0 - ratio;
}

/**
 * Get a color in a different shade.
 *
 * @return uint32_t - Adjusted color.
 */
uint32_t getShade()
{
  bool useNextColor = state == xFade;
  float ratio = getFadeRatio();

  uint8_t rgb[3];                // Resulting color.
  uint8_t bytes[3] = {16, 8, 0}; // R, G, B bytes.

  for (uint8_t i = 0; i <= 2; i++)
  {
    uint8_t byte = (currentColor >> bytes[i]) & 0xFF; // Separate the R, G, B bytes of the input color.
    uint8_t newValue = (uint8_t)(byte * ratio); // Set the brightness of the color.

    if(useNextColor) {
      uint8_t nextByte = (nextColor >> bytes[i]) & 0xFF; // Separate the R, G, B bytes of the input color.
      newValue = (uint8_t)((1.0f - ratio) * byte + ratio * nextByte); // Interpolate the color.
    }

    rgb[i] = newValue;
  }

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
    nextColor = getRGBColor(rgbIndex);

    LEDS.fill(currentColor, 0, LEDS_NUM);

    next();
    break;
  case fadeIn:
  case fadeOut:
  case xFade:
  {
    uint32_t shade = getShade();

    LEDS.fill(shade, 0, LEDS_NUM);
    LEDS.show();

    if (duration >= fade)
    {
      next();
    }
    break;
  }
  case holdColor:
    if (!isHolding)
    {
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