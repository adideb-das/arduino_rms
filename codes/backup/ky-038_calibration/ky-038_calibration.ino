// KY-038 sound sensor diagnostic
// Tells you: DC bias, noise floor, peak response, and whether the signal is alive

const uint8_t MIC_PIN = A0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  Serial.println(F("KY-038 diagnostic starting..."));
  Serial.println(F("Stay quiet for the first 3 seconds."));
  delay(3000);
}

void loop() {
  const uint16_t N = 1000;        // samples per window (~100 ms)
  uint16_t minVal = 1023;
  uint16_t maxVal = 0;
  uint32_t sum = 0;

  for (uint16_t i = 0; i < N; i++) {
    uint16_t v = analogRead(MIC_PIN);
    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
    sum += v;
  }

  uint16_t mean = sum / N;
  uint16_t pkpk = maxVal - minVal;

  Serial.print(F("min="));  
  Serial.print(minVal);
  Serial.print(F("  max=")); 
  Serial.print(maxVal);
  Serial.print(F("  mean="));
  Serial.print(mean);
  Serial.print(F("  pk-pk="));
  Serial.print(pkpk);

  // Quick interpretation
  if (pkpk < 3) {
    Serial.println(F("  [DEAD? signal not moving]"));
  } else if (pkpk < 15) {
    Serial.println(F("  [quiet — try clapping]"));
  } else if (pkpk < 100) {
    Serial.println(F("  [normal activity]"));
  } else {
    Serial.println(F("  [loud / saturating]"));
  }
}