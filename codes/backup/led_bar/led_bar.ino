const int  PIN_DATA = 11, PIN_CLK = 13, PIN_LATCH = 10;

void writeLeds(int pattern) {
  digitalWrite(PIN_LATCH, LOW);
  shiftOut(PIN_DATA, PIN_CLK, MSBFIRST, (pattern >> 8) & 0xFF);
  shiftOut(PIN_DATA, PIN_CLK, MSBFIRST,  pattern       & 0xFF);
  digitalWrite(PIN_LATCH, HIGH);
}


void setup() {
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
}

void loop() {
  
    // led bar graph , incrementing upto 10 bar led graph
  for (int n = 0; n <= 10; n++) {    
    int pattern = 0;
    for (int i = 0; i < n; i++) pattern |= (1u << i);
    writeLeds(pattern);
    delay(900);
  }


// writeLeds(0x3FF); delay(1000);   // all ten on at once
// writeLeds(0);     delay(400);   // all off
 
  
}



