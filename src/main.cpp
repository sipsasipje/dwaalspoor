#include <Arduino.h>
#include "RTC.h"
#include <NTPClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "secrets.h"
#include <Adafruit_NeoPixel.h>

#define LEDS_PIN 4
#define LEDS_NUM 300

enum LEDMode
{
  randomMode,
  seriesMode
};

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

#include "settings.h"

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
uint8_t colorIndex = 0; // Set color to: 0 - R, 1 - G, 2 - B or index of series.
bool isHolding = false;

/**
 * Get specific color or random color if not specified.
 *
 * @param uint8_t *rgb - RGB color values 0-255.
 */
uint32_t getColor(uint8_t *rgb = nullptr)
{
  if (rgb == nullptr)
  {
    uint8_t local[3];
    for (uint8_t i = 0; i <= 2; i++)
    {
      local[i] = random(0, 255);
    }
    rgb = local;
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
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
  ratio = constrain(ratio, 0.0, 1.0);

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
    uint8_t newValue = (uint8_t)(byte * ratio);       // Set the brightness of the color.

    if (useNextColor)
    {
      uint8_t nextByte = (nextColor >> bytes[i]) & 0xFF;              // Separate the R, G, B bytes of the input color.
      newValue = (uint8_t)((1.0f - ratio) * byte + ratio * nextByte); // Interpolate the color.
    }

    rgb[i] = newValue;
  }

  return LEDS.Color(rgb[0], rgb[1], rgb[2]);
}

void setColors()
{
  switch (ledMode)
  {
  case randomMode:
    currentColor = nextColor ? nextColor : getColor();
    nextColor = getColor();
    break;
  case seriesMode:
    currentColor = getColor(series[colorIndex]);
    colorIndex = (colorIndex + 1) % seriesLength;
    nextColor =  getColor(series[colorIndex]);
    break;
  }
}

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
    setColors();

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