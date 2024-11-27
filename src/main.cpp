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
bool isPaused = false; // Program is paused.
bool isSequenceEnd = false;

#define duration millis() - stamp
unsigned long stamp = 0; // Timestamp

State state;
uint32_t currentColor;
uint32_t nextColor;
uint8_t colorIndex = 0; // Set color to: 0 - R, 1 - G, 2 - B or index of series.
bool isHolding = false;

// WiFi settings.
int wifiStatus = WL_IDLE_STATUS;
WiFiUDP udp;

// Time settings.
NTPClient ntpcClient(udp, "nl.pool.ntp.org");

/**
 * Validate user settings.
 */
void validateSettings()
{
  auto checkTime = [](int time[2])
  {
    time[0] = time[0] % 24;
    time[1] = time[1] % 60;
  };

  checkTime(start);
  checkTime(end);
}

/*
 * Turn off the LED's and UV.
 */
void shutDown()
{
  LEDS.clear();
  LEDS.show();
  digitalWrite(UV_PIN, 0);
}

/**
 * Make WiFi connection.
 */
void connectWiFi()
{
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.println("Attempting to connect to Wifi.");
    wifiStatus = WiFi.begin(ssid, pass);
    delay(10000);
  }
  Serial.println("Connected to WiFi");
}

/**
 * Set the realtime clock.
 */
void setClock()
{
  RTC.begin();
  ntpcClient.begin();
  ntpcClient.update();

  auto unixTime = ntpcClient.getEpochTime() + (3600 * TIMEOFFSET);
  RTCTime rtcTime = RTCTime(unixTime);
  rtcTime.setSaveLight(SaveLight::SAVING_TIME_ACTIVE);

  RTC.setTime(rtcTime);
}

void togglePause();

/**
 * Set an alarm.
 *
 * @param int hour - Hour to set the alarm to.
 * @param int minutes - Minutes to set the alarm to.
 */
void setAlarm(int hour, int minutes)
{
  RTCTime alarmTime;
  alarmTime.setHour(hour);
  alarmTime.setMinute(minutes);

  AlarmMatch matchTime;
  matchTime.addMatchHour();
  matchTime.addMatchMinute();

  RTC.setAlarmCallback(togglePause, alarmTime, matchTime);
}

/**
 * Toggle the program pause state.
 */
void togglePause()
{
  isPaused = !isPaused;

  if (isPaused)
  {
    // shutDown();
    setAlarm(start[0], start[1]);
  }
  else
  {
    setAlarm(end[0], end[1]);
  }
}

/**
 * Convert hours and minutes to number of minutes.
 */
int toMinutes(int hour, int minutes)
{
  return hour * 60 + minutes;
}

/**
 * Check if the current time is between the set start and end time.
 */
bool isTimeBetween()
{
  RTCTime time;
  RTC.getTime(time);

  int currentMinutes = toMinutes(time.getHour(), time.getMinutes());
  int startMinutes = toMinutes(start[0], start[1]);
  int endMinutes = toMinutes(end[0], end[1]);

  bool isBetween = false;

  if (endMinutes <= startMinutes)
  {
    isBetween = currentMinutes >= startMinutes || currentMinutes <= endMinutes;
  }
  else
  {
    isBetween = currentMinutes >= startMinutes && currentMinutes <= endMinutes;
  }

  return isBetween;
}

/**
 * Check if the program should be paused or resumed.
 */
void checkPause()
{
  if (!isTimeBetween())
  {
    togglePause();
  }
  else
  {
    setAlarm(end[0], end[1]);
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
    nextColor = getColor(series[colorIndex]);
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

  if (!isPaused)
  {
    isSequenceEnd = false;
  }
  else if (sequenceIndex == 0 && stateIndex == 0 && repeatIndex == 0)
  {
    isSequenceEnd = true;
    shutDown();
  }

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
    ;

  validateSettings();

  pinMode(LEDS_PIN, OUTPUT);
  pinMode(UV_PIN, OUTPUT);

  if (AUTOPAUSE == true)
  {
    connectWiFi();
    setClock();
    checkPause();
  }

  LEDS.begin();
  setState();
}

/**
 * Main loop.
 */
void loop()
{
  if (isPaused && isSequenceEnd)
    return;

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