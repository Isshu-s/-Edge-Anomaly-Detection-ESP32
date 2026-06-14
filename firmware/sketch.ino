#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DHT_PIN 15
#define LED_PIN 2
#define DHT_TYPE DHT22

#define WINDOW_SIZE 20
#define Z_THRESHOLD 1.5f

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

float buf[WINDOW_SIZE];
int writeIdx = 0;
int sampleCnt = 0;
int faultTotal = 0;
unsigned long startMs = 0;

float rollingMean() {
  int n = min(sampleCnt, WINDOW_SIZE);
  float s = 0;

  for (int i = 0; i < n; i++) {
    s += buf[i];
  }

  return s / n;
}

float rollingStdDev(float mean) {
  int n = min(sampleCnt, WINDOW_SIZE);
  float s = 0;

  for (int i = 0; i < n; i++) {
    s += pow(buf[i] - mean, 2);
  }

  return sqrt(s / n);
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  oled.setCursor(10, 10);
  oled.println("Edge Fault Detector");

  oled.setCursor(10, 26);
  oled.println("ESP32 + DHT22");

  oled.setCursor(10, 40);
  oled.println("Initializing...");

  oled.display();

  delay(2000);

  startMs = millis();

  Serial.println("TIME_S,TEMP_C,MEAN_C,STDDEV,ZSCORE,FAULT");
}

void loop() {

  float rawTemp = dht.readTemperature();

  if (isnan(rawTemp)) {
    delay(500);
    return;
  }

  float t = millis() / 1000.0f;

  float injection = 0;

  if (fmod(t, 10.0) < 3.0) {
    injection = 40.0f;
  }

  float measured = rawTemp + injection;

  buf[writeIdx % WINDOW_SIZE] = measured;

  writeIdx++;
  sampleCnt++;

  if (sampleCnt < 5) {
    delay(500);
    return;
  }

  float m = rollingMean();
  float sd = rollingStdDev(m);

  float z = (sd > 0.01f)
              ? fabs(measured - m) / sd
              : 0.0f;

  bool fault = (z > Z_THRESHOLD);

  if (fault) {
    faultTotal++;
  }

  digitalWrite(LED_PIN, fault);

  float elapsed = (millis() - startMs) / 1000.0f;

  oled.clearDisplay();

  oled.setTextSize(1);

  oled.setCursor(0, 0);
  oled.print("Temp:");
  oled.print(measured, 1);

  oled.setCursor(0, 10);
  oled.print("Mean:");
  oled.print(m, 1);

  oled.setCursor(0, 20);
  oled.print("Std:");
  oled.print(sd, 2);

  oled.setCursor(0, 30);
  oled.print("Z:");
  oled.print(z, 2);

  oled.setCursor(0, 44);

  if (fault) {
    oled.setTextSize(2);
    oled.println("FAULT");
    oled.setTextSize(1);
  } else {
    oled.print("NORMAL");
  }

  oled.setCursor(70, 56);
  oled.print("F:");
  oled.print(faultTotal);

  oled.display();

  Serial.print(elapsed, 2);
  Serial.print(",");
  Serial.print(measured, 2);
  Serial.print(",");
  Serial.print(m, 2);
  Serial.print(",");
  Serial.print(sd, 2);
  Serial.print(",");
  Serial.print(z, 2);
  Serial.print(",");
  Serial.println(fault ? 1 : 0);

  delay(500);
}

