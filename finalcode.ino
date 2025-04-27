#include <Wire.h>
#include <MPU6050.h>
#include <math.h>
#include "BluetoothSerial.h"

// FLEX SENSOR SETUP
const int flexPins[5] = {25, 33, 32, 35, 34};
const float thresholds[5] = {1.25, 1.07, 1.65, 1.30, 1.38};
int flexStates[5];
const float waveZeroCrossThreshold = 2;
const float swipeThreshold = 0.4;
const float softMotionThreshold = 0.25;
const float motionStillThreshold = 0.5;
const float upwardThreshold = 0.25;

BluetoothSerial SerialBT;

struct NumberPattern {
  int id;
  int pattern[5];
};

NumberPattern patterns[] = {
  {1, {0, 1, 0, 0, 0}},
  {2, {0, 1, 1, 0, 0}},
  {3, {0, 1, 1, 1, 0}},
  {4, {0, 1, 1, 1, 1}},
  {5, {1, 1, 1, 1, 1}},
  {6, {1, 0, 0, 0, 0}},
  {7, {1, 1, 0, 0, 0}},
  {8, {1, 1, 1, 0, 0}},
  {9, {1, 1, 1, 1, 0}},
  {10, {0, 0, 0, 0, 0}}
};

// MPU6050 SETUP
MPU6050 mpu;
const int bufferSize = 50;
float axBuffer[bufferSize], ayBuffer[bufferSize], azBuffer[bufferSize];
int bufferIndex = 0;

int detectedNumber = 0;
int previousDetectedNumber = 0;
unsigned long lastOutputTime = 0;
unsigned long lastGestureTime = 0;
const unsigned long gestureCooldown = 1000;


void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();
  SerialBT.begin("ESP32");

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 not connected!");
    while (1);
  }

  Serial.println("MPU6050 Initialized. Flex Sensors Ready.");
  for (int i = 0; i < 5; i++) {
    pinMode(flexPins[i], INPUT);
  }
}

void loop() {
  storeAccelData();
  readFlexSensors();
  // detectCombinedWords();
  detectComplexGestures();
  delay(10); // ~66 Hz
}

// FLEX FUNCTIONS
void readFlexSensors() {
  for (int i = 0; i < 5; i++) {
    int sensorValue = analogRead(flexPins[i]);
    float voltage = sensorValue * (3.3 / 4095.0);
    flexStates[i] = (voltage < thresholds[i]) ? 0 : 1;
  }
  previousDetectedNumber = detectedNumber;
  detectedNumber = matchPattern();
}

int matchPattern() {
  for (auto &pattern : patterns) {
    bool match = true;
    for (int i = 0; i < 5; i++) {
      if (flexStates[i] != pattern.pattern[i]) {
        match = false;
        break;
      }
    }
    if (match) return pattern.id;
  }
  return 0;
}

// MPU FUNCTIONS
void storeAccelData() {
  int16_t axRaw, ayRaw, azRaw;
  mpu.getAcceleration(&axRaw, &ayRaw, &azRaw);

  float ax = axRaw / 16384.0;
  float ay = ayRaw / 16384.0;
  float az = azRaw / 16384.0;

  float pitch = atan2(ax, sqrt(ay * ay + az * az));
  float roll  = atan2(ay, sqrt(ax * ax + az * az));

  float axWorld = ax;
  float ayWorld = ay * cos(roll) - az * sin(roll);
  float azWorld = ay * sin(roll) + az * cos(roll);

  float axCorrected = axWorld * cos(pitch) + azWorld * sin(pitch);
  float azCorrected = -axWorld * sin(pitch) + azWorld * cos(pitch);
  float ayCorrected = ayWorld;
  azCorrected -= 1.0;

  axBuffer[bufferIndex] = axCorrected;
  ayBuffer[bufferIndex] = ayCorrected;
  azBuffer[bufferIndex] = azCorrected;

  bufferIndex = (bufferIndex + 1) % bufferSize;
}
bool isMostlyStill() {
  for (int i = 0; i < bufferSize; i++) {
    if (fabs(axBuffer[i]) > motionStillThreshold ||
        fabs(ayBuffer[i]) > motionStillThreshold ||
        fabs(azBuffer[i]) > motionStillThreshold) {
      return false;
    }
  }
  return true;
}

void detectComplexGestures() {
  if (millis() - lastGestureTime < gestureCooldown) return;
  if (isMostlyStill()) return;

  // Calculate energies
  float axEnergy = meanEnergy(axBuffer);
  float ayEnergy = meanEnergy(ayBuffer);
  float azEnergy = meanEnergy(azBuffer);

  float totalEnergy = axEnergy + ayEnergy + azEnergy;

  if (totalEnergy < 0.02) {
    // Very small movement
    return;
  }

  float xShare = axEnergy / totalEnergy;
  float yShare = ayEnergy / totalEnergy;
  // No need to use zShare

  // --- FIRST: Try wave gesture separately ---
  if (detectWaveGesture()) {
    if(detectedNumber==4){SerialBT.println("Hello"); Serial.println("Hello");}
    if(detectedNumber==5){SerialBT.println("Bye"); }
    if(detectedNumber==8){SerialBT.println("What");}
    if(detectedNumber==7){SerialBT.println("Foolish");}
    lastGestureTime = millis();
    return;
  }

  // --- SECOND: Try forward/backward if x axis is dominant ---
  if (xShare > 0.4) {  // X-dominant motion
    if (detectForwardMotion()) {
      if(detectedNumber==1){SerialBT.println("You");}
      if(detectedNumber==4){SerialBT.println("Okay");}
      lastGestureTime = millis();
      return;
    } 
    else if (detectBackwardMotion()) {
      if(detectedNumber==6){SerialBT.println("Me");}
      if(detectedNumber==4){SerialBT.println("Welcome");}
      lastGestureTime = millis();
      return;
    }
  }

  // --- THIRD: Try upward/downward using azDelta (NO zShare) ---
  float azDelta = meanDelta(azBuffer);

  if (azDelta > upwardThreshold) {  // upward motion detected
    if(detectedNumber==5){SerialBT.println("Happy");}
    if(detectedNumber==1){SerialBT.println("Tall");
    if(detectedNumber==6){SerialBT.println("Good");}
    lastGestureTime = millis();
    return;
  } else if (azDelta < -upwardThreshold) {  // downward motion detected
      if(detectedNumber==4){SerialBT.println("Thank You");
      if(detectedNumber==6){SerialBT.println("Bad");}
      if(detectedNumber==10){SerialBT.println("Please");}
      if(detectedNumber==5){SerialBT.println("Sad");}
      lastGestureTime = millis();
      return;
    }
  }

float meanEnergy(float* arr) {
  float sum = 0;
  for (int i = 0; i < bufferSize; i++) {
    sum += arr[i] * arr[i];
  }
  return sum / bufferSize;
}

float meanDelta(float* arr) {
  float recentSum = 0, previousSum = 0;
  int recentCount = 15;
  int previousCount = 15;

  for (int i = 0; i < previousCount; i++) {
    int index = (bufferIndex + i) % bufferSize;
    previousSum += arr[index];
  }

  for (int i = previousCount; i < previousCount + recentCount; i++) {
    int index = (bufferIndex + i) % bufferSize;
    recentSum += arr[index];
  }

  float previousMean = previousSum / previousCount;
  float recentMean = recentSum / recentCount;

  return recentMean - previousMean;
}
bool detectWaveGesture() {
  int zeroCrosses = 0;
  for (int i = 5; i < bufferSize - 5; i++) {
    float prevAvg = 0.0;
    float nextAvg = 0.0;

    for (int j = i - 5; j < i; j++) {
      prevAvg += ayBuffer[j];
    }
    prevAvg /= 5.0;

    for (int j = i; j < i + 5; j++) {
      nextAvg += ayBuffer[j];
    }
    nextAvg /= 5.0;

    if ((prevAvg > 0 && nextAvg < 0) || (prevAvg < 0 && nextAvg > 0)) {
      if (fabs(nextAvg - prevAvg) > 0.05) {
        zeroCrosses++;
      }
    }
  }
  return zeroCrosses >= waveZeroCrossThreshold;
}

bool repeatedUpDown() {
  int zeroCrosses = 0;
  for (int i = 5; i < bufferSize - 5; i++) {
    float prevAvg = 0.0;
    float nextAvg = 0.0;

    for (int j = i - 5; j < i; j++) {
      prevAvg += azBuffer[j];
    }
    prevAvg /= 5.0;

    for (int j = i; j < i + 5; j++) {
      nextAvg += azBuffer[j];
    }
    nextAvg /= 5.0;

    if ((prevAvg > 0 && nextAvg < 0) || (prevAvg < 0 && nextAvg > 0)) {
      if (fabs(nextAvg - prevAvg) > 0.05) {
        zeroCrosses++;
      }
    }
  }
  return zeroCrosses >= 2; // You can tune this threshold
}

bool detectHorizontalSideSwipe() {
  return fabs(meanDelta(ayBuffer)) > swipeThreshold;
}

bool detectUpwardMotion() {
  return meanDelta(azBuffer) > upwardThreshold;
}

bool detectDownwardMotion() {
  return meanDelta(azBuffer) < -upwardThreshold;
}

bool detectForwardMotion() {
  return meanDelta(axBuffer) > softMotionThreshold;
}

bool detectBackwardMotion() {
  return meanDelta(axBuffer) < -softMotionThreshold;
}

// bool detectForwardMotion() {
//   bool motion = meanDelta(axBuffer) > 0.2;
//   if (motion) {
//     if (detectedNumber == 1) Serial.println("You");
//     else if (detectedNumber == 6) Serial.println("Good");
//     else if (previousDetectedNumber == 10 && detectedNumber == 5) Serial.println("Hearing");
//   }
//   return motion;
// }

// bool detectBackwardMotion() {
//   bool motion = meanDelta(axBuffer) < -0.2;
//   if (motion) {
//     if (detectedNumber == 6) Serial.println("Me");
//     else if (detectedNumber == 4) Serial.println("Welcome");
//   }
//   return motion;
// }

// bool detectUpwardMotion() {
//   bool motion = meanDelta(azBuffer) > 0.25;
//   if (motion) {
//     if (previousDetectedNumber == 10 && detectedNumber == 5) Serial.println("Morning");
//     else if (detectedNumber == 5) Serial.println("Happy");
//     else if (detectedNumber == 1) Serial.println("Tall");
//   }
//   return motion;
// }

// bool detectDownwardMotion() {
//   bool motion = meanDelta(azBuffer) < -0.25;
//   if (motion) {
//     if (detectedNumber == 4) Serial.println("Thank You");
//     else if (detectedNumber == 10) Serial.println("Please");
//     else if (detectedNumber == 5) Serial.println("Sad");
//     else if (previousDetectedNumber == 10 && detectedNumber == 5) Serial.println("Bad");
//   }
//   return motion;
// }

// bool detectWaveGesture() {
//   int zeroCrosses = 0;
//   for (int i = 5; i < bufferSize - 5; i++) {
//     float prevAvg = 0.0, nextAvg = 0.0;
//     for (int j = i - 5; j < i; j++) prevAvg += ayBuffer[j];
//     for (int j = i; j < i + 5; j++) nextAvg += ayBuffer[j];
//     prevAvg /= 5.0;
//     nextAvg /= 5.0;
//     if ((prevAvg > 0 && nextAvg < 0) || (prevAvg < 0 && nextAvg > 0)) {
//       if (fabs(nextAvg - prevAvg) > 0.1) zeroCrosses++;
//     }
//   }

//   if (zeroCrosses >= 2) {
//     if (detectedNumber == 4) Serial.println("Hello");
//     else if (detectedNumber == 5) Serial.println("Bye Bye");
//     else if (detectedNumber == 8) Serial.println("What");
//     else if (detectedNumber == 7) Serial.println("Foolish");
//     return true;
//   }

//   return false;
// }

// bool repeatedUpDown() {
//   float delta = meanDelta(azBuffer);
//   return fabs(delta) > 0.2;
// }

// void detectCombinedWords() {
//   // Only proceed if cooldown has expired
//   if (millis() - lastGestureTime < gestureCooldown) return;
// //forward
//   bool motion = meanDelta(axBuffer) > 0.2;
//   if (motion) {
//     if (detectedNumber == 1){ Serial.println("You"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 6) {Serial.println("Good"); lastGestureTime = millis(); return;}
//     else if (previousDetectedNumber == 10 && detectedNumber == 5) {Serial.println("Hearing"); lastGestureTime = millis(); return;}
//   }
//   //backward
//   motion = meanDelta(axBuffer) < -0.2;
//   if (motion) {
//     if (detectedNumber == 6) {Serial.println("Me"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 4) {Serial.println("Welcome"); lastGestureTime = millis(); return;}
//   }

//   //upward
//   motion = meanDelta(azBuffer) > 0.25;
//   if (motion) {
//     if (previousDetectedNumber == 10 && detectedNumber == 5) {Serial.println("Morning"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 5) {Serial.println("Happy"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 1) {Serial.println("Tall"); lastGestureTime = millis(); return;}
//   }
//   motion = meanDelta(azBuffer) < -0.25;
//   if (motion) {
//     if (detectedNumber == 4) {Serial.println("Thank You"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 10) {Serial.println("Please"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 5) {Serial.println("Sad"); lastGestureTime = millis(); return;}
//     else if (previousDetectedNumber == 10 && detectedNumber == 5) {Serial.println("Bad"); lastGestureTime = millis(); return;}
//   }
//   int zeroCrosses = 0;
//   for (int i = 5; i < bufferSize - 5; i++) {
//     float prevAvg = 0.0, nextAvg = 0.0;
//     for (int j = i - 5; j < i; j++) prevAvg += ayBuffer[j];
//     for (int j = i; j < i + 5; j++) nextAvg += ayBuffer[j];
//     prevAvg /= 5.0;
//     nextAvg /= 5.0;
//     if ((prevAvg > 0 && nextAvg < 0) || (prevAvg < 0 && nextAvg > 0)) {
//       if (fabs(nextAvg - prevAvg) > 0.1) zeroCrosses++;
//     }
//   }

//   if (zeroCrosses >= 2) {
//     if (detectedNumber == 4) {Serial.println("Hello"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 5) {Serial.println("Bye Bye"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 8) {Serial.println("What"); lastGestureTime = millis(); return;}
//     else if (detectedNumber == 7) {Serial.println("Foolish"); lastGestureTime = millis(); return;}
//   }

//   if (detectedNumber == 8 && fabs(meanDelta(azBuffer)) > 0.2) Serial.println("Again");
//   else if (detectedNumber == 8 && repeatedUpDown()) Serial.println("Why");
//   else if (flexStates[0] == 0 && flexStates[1] == 0 && flexStates[2] == 1 && flexStates[3] == 1 && flexStates[4] == 1) Serial.println("Okay");

//   lastGestureTime = millis(); return; // Reset cooldown
// }