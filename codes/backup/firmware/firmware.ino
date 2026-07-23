//test 2

#include <Arduino.h>
//Assigned Pins
const uint8_t PIN_AUDIO = A0;   // KY-038 analog output
const uint8_t PIN_DATA  = 11;   // 74HC595 SER  (pin 14)
const uint8_t PIN_CLK   = 13;   // 74HC595 SCLK (pin 11)
const uint8_t PIN_LATCH = 10;   // 74HC595 RCLK (pin 12)

//Sampling
const uint16_t SAMPLE_RATE_HZ   = 8000;
const uint16_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ; // 125 us
const uint16_t BUF_LEN          = 128;   // 16 ms RMS window @ 8 kHz; MUST be a power of two
const uint16_t BUF_MASK         = BUF_LEN - 1;

int16_t  sampleBuf[BUF_LEN];
uint16_t bufIdx       = 0;
uint32_t sumSquares   = 0;
uint32_t nextSampleAt = 0;

int32_t dcEstimate = 512L << 8;

//Signal-health telemetry
uint16_t rawMin = 1023;
uint16_t rawMax = 0;

//Display
const uint8_t  NUM_LEDS          = 10;
const uint16_t DISPLAY_PERIOD_MS = 25;
int16_t sensitivity = 120;
uint32_t nextDisplayAt = 0;
const bool SERIAL_DEBUG = true;
uint32_t nextDebugAt = 0;

//Peak-hold state
float    peakLevel      = 0;      // 0..NUM_LEDS, fractional for smooth decay
uint32_t peakHoldUntil  = 0;
uint32_t lastPeakUpdate = 0;
const uint16_t PEAK_HOLD_MS       = 2000;  // freeze time before decay starts
const float    PEAK_DECAY_PER_SEC = 4.0;  // LED segments/sec fall rate — tune this

void writeLeds(uint16_t pattern) {
  digitalWrite(PIN_LATCH, LOW);
  shiftOut(PIN_DATA, PIN_CLK, MSBFIRST, (pattern >> 8) & 0xFF);
  shiftOut(PIN_DATA, PIN_CLK, MSBFIRST,  pattern       & 0xFF);
  digitalWrite(PIN_LATCH, HIGH);
}

void setup() {
  pinMode(PIN_DATA,  OUTPUT);
  pinMode(PIN_CLK,   OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);

  ADCSRA = (ADCSRA & 0xF8) | 0x05;

  Serial.begin(115200);
  Serial.println(F("Audio RMS meter ready."));
  Serial.println(F("Send '+' for more sensitivity, '-' for less."));

  for (uint16_t i = 0; i < BUF_LEN; i++) sampleBuf[i] = 0;

  writeLeds(0);
  nextSampleAt   = micros();
  nextDisplayAt  = millis();
  lastPeakUpdate = millis();
}

void loop() {
  //User interface
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '+')      sensitivity = max(10,   sensitivity - 10);
    else if (c == '-') sensitivity = min(1000, sensitivity + 10);
    if (c == '+' || c == '-') {
      Serial.print(F("sensitivity = ")); Serial.println(sensitivity);
    }
  }

  //Audio sampling at a fixed rate 
  if ((int32_t)(micros() - nextSampleAt) >= 0) {
    int32_t late = (int32_t)(micros() - nextSampleAt);
    if (late > (int32_t)(4 * SAMPLE_PERIOD_US)) nextSampleAt = micros();
    else                                        nextSampleAt += SAMPLE_PERIOD_US;

    int16_t raw = analogRead(PIN_AUDIO);          // 0..1023

    //signal-health tracking on the RAW signal
    if ((uint16_t)raw < rawMin) rawMin = raw;
    if ((uint16_t)raw > rawMax) rawMax = raw;

    dcEstimate += (((int32_t)raw << 8) - dcEstimate) >> 9;
    int16_t hp = raw - (int16_t)(dcEstimate >> 8);

    int32_t oldS = sampleBuf[bufIdx];
    sumSquares -= (uint32_t)(oldS * oldS);
    sumSquares += (uint32_t)((int32_t)hp * hp);
    sampleBuf[bufIdx] = hp;
    bufIdx = (bufIdx + 1) & BUF_MASK;
  }

  //Display update
  uint32_t now = millis();
  if ((int32_t)(now - nextDisplayAt) >= 0) {
    nextDisplayAt = now + DISPLAY_PERIOD_MS;

    float rms = sqrt((float)sumSquares / BUF_LEN);

    float level = rms * NUM_LEDS / sensitivity;
    if (level > NUM_LEDS) level = NUM_LEDS;
    if (level < 0)        level = 0;

    uint8_t  barLeds = (uint8_t)(level + 0.5);
    uint16_t pattern = 0;
    for (uint8_t i = 0; i < barLeds; i++) pattern |= (1u << i);

    //Peak-hold with gradual decay 
    uint32_t dtMs = now - lastPeakUpdate;
    lastPeakUpdate = now;

    if (level >= peakLevel) {
      peakLevel     = level;
      peakHoldUntil = now + PEAK_HOLD_MS;
    } else if ((int32_t)(now - peakHoldUntil) >= 0) {
      peakLevel -= PEAK_DECAY_PER_SEC * (dtMs / 1000.0f);
      if (peakLevel < level) peakLevel = level;  // don't let marker sink into the live bar
    }
    if (peakLevel < 0) peakLevel = 0;

    uint8_t peakLedIndex = (uint8_t)(peakLevel + 0.5);
    if (peakLedIndex > barLeds && peakLedIndex >= 1 && peakLedIndex <= NUM_LEDS) {
      pattern |= (1u << (peakLedIndex - 1));   // single marker LED above the solid bar
    }

    writeLeds(pattern);

    //Debug + signal-health output
    if (SERIAL_DEBUG && (int32_t)(now - nextDebugAt) >= 0) {
      nextDebugAt = now + 250;

      uint16_t pkpk = rawMax - rawMin;
      uint16_t dc   = (uint16_t)(dcEstimate >> 8);

      Serial.print(F("RMS: "));         Serial.print(rms, 1);
      Serial.print(F("  sensitivity: ")); Serial.print(sensitivity);
      Serial.print(F("  LEDs: "));      Serial.print(barLeds);
      Serial.print(F("  peak: "));      Serial.print(peakLevel, 1);
      Serial.print(F("  dc: "));        Serial.print(dc);
      Serial.print(F("  raw pk-pk: ")); Serial.print(pkpk);

      if (pkpk < 3)        Serial.println(F("  [MIC DEAD? A0 flat]"));
      else if (pkpk < 15)  Serial.println(F("  [quiet]"));
      else                 Serial.println(F("  [signal alive]"));

      rawMin = 1023;
      rawMax = 0;
    }
  }
}