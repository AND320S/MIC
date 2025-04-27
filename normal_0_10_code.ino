const int flexPin[5] = {25, 33, 32, 35, 34}; //Defining Input Pins

bool comarr(int bin[], int pattern[]) { //Custom Fucntions 
  for (int j = 0; j < 5; j++) {
    if (bin[j] != pattern[j]) {
      return false;
    }
  }
  return true;
}
 
const float thr[5] = {1.25, 1.07, 1.65, 1.30, 1.38};  // keep float type for thresholds

int numberPatterns[10][5] = {
  {0, 1, 0, 0, 0},  // 1
  {0, 1, 1, 0, 0},  // 2
  {0, 1, 1, 1, 0},  // 3
  {0, 1, 1, 1, 1},  // 4
  {1, 1, 1, 1, 1},  // 5
  {1, 0, 0, 0, 0},  // 6
  {1, 1, 0, 0, 0},  // 7
  {1, 1, 1, 0, 0},  // 8
  {1, 1, 1, 1, 0},  // 9
  {0, 0, 0, 0, 0}   // 10
};

int bin[5] = {1, 1, 1, 1, 1};  // 5th = 0 by default

void setup() {
  Serial.begin(115200);  // ESP32 supports higher rates too
}

void loop() {
  // Reset bin to all 1s except last
  for (int i = 0; i < 5; i++) {
    bin[i] = 1;
  }
  

  float val[5];

  for (int i = 0; i < 5; i++) {
    int sensorval = analogRead(flexPin[i]);
    val[i] = sensorval * (3.3 / 4095.0);  // ESP32 ADC
    if (val[i] < thr[i]) {
      bin[i] = 0;
    }
  }

  int num = 0;
  for (int i = 0; i < 10; i++) {
    if (comarr(bin, numberPatterns[i])) {
      num = i + 1;
      break;
    }
  }

  for (int i = 0; i < 5; i++) {
    Serial.print(val[i], 2); Serial.print(" ");
  }
  Serial.print("=> Number: ");
  Serial.println(num);

  delay(1000);
}