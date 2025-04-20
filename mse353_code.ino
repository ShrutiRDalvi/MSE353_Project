#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin definitions
const int trigPin = 2;
const int echoPin = 3;
const int waterPumpRelayPin = 4;  // IN1
const int buzzerPin = 5;
const int tempPin = 6;
const int dosingPumpRelayPin = 7; // IN2
const int turbidityPin = A0;

// Ultrasonic thresholds
const int lowLevelDistance = 15;
const int highLevelDistance = 8;

// Turbidity smoothing
const int numReadings = 20;
int readings[numReadings];
int readIndex = 0;

// DS18B20 setup
OneWire oneWire(tempPin);
DallasTemperature sensors(&oneWire);

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(waterPumpRelayPin, OUTPUT);
  pinMode(dosingPumpRelayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(waterPumpRelayPin, HIGH);   // Water pump OFF
  digitalWrite(dosingPumpRelayPin, HIGH);  // Dosing pump OFF
  digitalWrite(buzzerPin, LOW);

  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.backlight();

  sensors.begin();

  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }
}

long readDistanceCM() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  return duration == 0 ? -1 : duration * 0.034 / 2;
}

int getSmoothedTurbidity() {
  for (int i = 0; i < numReadings; i++) {
    readings[i] = analogRead(turbidityPin);
    delay(5);
  }
  for (int i = 0; i < numReadings - 1; i++) {
    for (int j = i + 1; j < numReadings; j++) {
      if (readings[i] > readings[j]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  long sum = 0;
  for (int i = 2; i < numReadings - 2; i++) {
    sum += readings[i];
  }
  return sum / (numReadings - 4);
}

String getWaterQuality(int turbidity) {
  if (turbidity < 400) return "Dirty";
  else if (turbidity < 700) return "Medium";
  else return "Clean";
}

void loop() {
  long distance = readDistanceCM();
  int turbidity = getSmoothedTurbidity();
  String quality = getWaterQuality(turbidity);

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Serial/Bluetooth output
  Serial.print("Distance: ");
  Serial.print(distance == -1 ? "Error" : String(distance) + " cm");
  Serial.print(" | Turbidity: ");
  Serial.print(turbidity);
  Serial.print(" | Temp: ");
  Serial.print(tempC);
  Serial.print(" C | Quality: ");
  Serial.println(quality);

  // LCD display
  lcd.setCursor(0, 0);
  lcd.print("D:");
  if (distance == -1) lcd.print("Err");
  else lcd.print(String(distance) + "cm ");
  lcd.print("T:");
  lcd.print(tempC);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Turb:");
  lcd.print(turbidity);
  lcd.print(" ");
  lcd.print(quality);
  lcd.print("   ");

  // Water pump logic
  if (distance > lowLevelDistance) {
    digitalWrite(waterPumpRelayPin, LOW);  // Water pump ON
    digitalWrite(buzzerPin, HIGH);
  } else if (distance >= 0 && distance < highLevelDistance) {
    digitalWrite(waterPumpRelayPin, HIGH); // Water pump OFF
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);          // No alert
  }

  // Dosing pump logic (when water is dirty)
  if (turbidity < 400) {
    digitalWrite(dosingPumpRelayPin, LOW);  // Dosing pump ON
  } else {
    digitalWrite(dosingPumpRelayPin, HIGH); // Dosing pump OFF
  }

  delay(1000);
}