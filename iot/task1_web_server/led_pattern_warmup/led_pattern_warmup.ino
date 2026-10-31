int leds[] = {8, 9, 10, 11};

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(leds[i], OUTPUT);
  }
}

void allOff() {
  for (int i = 0; i < 4; i++)
    digitalWrite(leds[i], LOW);
}

void allOn() {
  for (int i = 0; i < 4; i++)
    digitalWrite(leds[i], HIGH);
}

void chase(int delayTime) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(leds[i], HIGH);
    delay(delayTime);
    digitalWrite(leds[i], LOW);
  }
}

void reverseChase(int delayTime) {
  for (int i = 3; i >= 0; i--) {
    digitalWrite(leds[i], HIGH);
    delay(delayTime);
    digitalWrite(leds[i], LOW);
  }
}

void beat() {
  allOn();
  delay(70);
  allOff();
  delay(80);
}

void loop() {

  // Intro
  chase(120);
  reverseChase(120);

  // Verse
  for (int i = 0; i < 4; i++) {
    chase(90);
    delay(100);
  }

  // Build-up
  for (int i = 0; i < 4; i++) {
    chase(55);
    reverseChase(55);
  }

  // Chorus
  for (int i = 0; i < 8; i++) {
    beat();

    digitalWrite(leds[0], HIGH);
    delay(50);
    digitalWrite(leds[0], LOW);

    digitalWrite(leds[3], HIGH);
    delay(50);
    digitalWrite(leds[3], LOW);
  }

  // Fast section
  for (int i = 0; i < 12; i++) {
    chase(25);
    reverseChase(25);
  }

  // Ending
  allOn();
  delay(250);
  allOff();
  delay(500);
}
