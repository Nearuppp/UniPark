// === Pin configuration ===
const int hallPin = 22;
const int redPin = 10;
const int greenPin = 9;
const int bluePin = 8;

// === State variables ===
bool magneticDetected = false;
bool inYellowPhase = false;
bool inGreenPhase = false;

unsigned long yellowStartTime = 0;
unsigned long lastBlinkTime = 0;
bool ledOn = false;

void setup() {
  Serial.begin(9600);

  pinMode(hallPin, INPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  setRGB(0, 0, 255);  // Start with LED off Green
}

void loop() {
  int sensorState = digitalRead(hallPin);  // LOW = magnet near
  unsigned long currentTime = millis();

  if (sensorState == LOW) {
    if (!magneticDetected) {
      // First time magnet is detected
      magneticDetected = true;
      inYellowPhase = true;
      inGreenPhase = false;
      yellowStartTime = currentTime;
      lastBlinkTime = currentTime;
      Serial.println("Magnet detected → Yellow phase");
    }

    if (inYellowPhase && (currentTime - yellowStartTime < 5000)) {
      // Blinking yellow for 5 seconds
      if (currentTime - lastBlinkTime >= 200) {
        lastBlinkTime = currentTime;
        ledOn = !ledOn;
        if (ledOn) {
          setRGB(255, 0, 255);  // Yellow
        } else {
          setRGB(0, 0, 0);
        }
      }
    } else if (inYellowPhase) {
      // Switch from yellow to green blinking
      inYellowPhase = false;
      inGreenPhase = true;
      lastBlinkTime = currentTime;
      ledOn = false;
      setRGB(0, 0, 0);
      Serial.println("Switching to Green phase");
    }

    if (inGreenPhase && (currentTime - lastBlinkTime >= 200)) {
      lastBlinkTime = currentTime;
      ledOn = !ledOn;
      if (ledOn) {
        setRGB(255, 0, 0);  // Green
      } else {
        setRGB(0, 0, 0);
      }
    }

  } else {
    // No magnet
    if (magneticDetected) {
      magneticDetected = false;
      inYellowPhase = false;
      inGreenPhase = false;
      ledOn = false;
      setRGB(0, 0, 0);
      Serial.println("Magnet removed → LED off");
    }
    setRGB(0, 0, 255);
  }

  delay(10);  // Smooth loop
}

void setRGB(int redValue, int greenValue, int blueValue) {
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}
