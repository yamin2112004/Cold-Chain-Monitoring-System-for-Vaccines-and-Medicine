#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// ------------------- PIN DEFINITIONS -------------------
#define DHT_PIN 4
#define DHT_TYPE DHT22

#define LED_GREEN 25
#define LED_RED 26
#define LED_YELLOW 27
#define BUZZER_PIN 14
#define ACK_BUTTON 33

#define DOOR_SENSOR_PIN 34   // Potentiometer (ADC)

// ------------------- SAFE LIMITS (COLD CHAIN) -------------------
#define TEMP_MIN 2.0
#define TEMP_MAX 8.0
#define HUM_MAX 60.0
#define DOOR_OPEN_THRESHOLD 2000

// ------------------- WIFI & CLOUD -------------------
const char* ssid = "Wokwi-GUEST";
const char* password = "";
String apiKey = "R4WIFWQIHOFTQTQ9";

// ------------------- OBJECTS -------------------
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------- VARIABLES -------------------
bool alarmActive = false;
bool buzzerMuted = false;

unsigned long lastBlink = 0;
bool yellowState = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ACK_BUTTON, INPUT_PULLUP);
  pinMode(DOOR_SENSOR_PIN, INPUT);

  dht.begin();
  lcd.init();
  lcd.backlight();

  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  noTone(BUZZER_PIN);

  Serial.println("Cold-Chain Vaccine Monitoring System");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

void loop() {

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int doorValue = analogRead(DOOR_SENSOR_PIN);

  bool unsafe = false;
  bool doorOpen = false;

  // ------------------- SENSOR VALIDATION -------------------
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT Sensor Error");
    delay(2000);
    return;
  }

  // ------------------- SAFETY CHECK -------------------
  if (temperature < TEMP_MIN || temperature > TEMP_MAX || humidity > HUM_MAX) {
    unsafe = true;
  }

  // ------------------- DOOR CHECK -------------------
  if (doorValue > DOOR_OPEN_THRESHOLD) {
    doorOpen = true;
  }

  // ------------------- ALARM LOGIC -------------------
  alarmActive = unsafe;
  if (!unsafe) buzzerMuted = false;

  // ------------------- ACK BUTTON -------------------
  if (digitalRead(ACK_BUTTON) == LOW) {
    buzzerMuted = true;
    noTone(BUZZER_PIN);
    delay(300);
  }

  // ------------------- OUTPUT CONTROL -------------------
  if (alarmActive) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    if (!buzzerMuted) {
      tone(BUZZER_PIN, 800);
    }
  } else {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    noTone(BUZZER_PIN);
  }

  // ------------------- DOOR BLINK (YELLOW LED) -------------------
  if (doorOpen) {
    if (millis() - lastBlink > 500) {
      yellowState = !yellowState;
      digitalWrite(LED_YELLOW, yellowState);
      lastBlink = millis();
    }
  } else {
    digitalWrite(LED_YELLOW, LOW);
  }

  // ------------------- LCD DISPLAY -------------------
  lcd.clear();
  if (alarmActive) {
    lcd.setCursor(0, 0);
    lcd.print("WARNING!");
    lcd.setCursor(0, 1);
    lcd.print("UNSAFE STORAGE");
  } 
  else if (doorOpen) {
    lcd.setCursor(0, 0);
    lcd.print("DOOR OPEN");
    lcd.setCursor(0, 1);
    lcd.print("Check Storage");
  }
  else {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C ");

    lcd.print("H:");
    lcd.print(humidity, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Storage SAFE");
  }

  // ------------------- CLOUD UPDATE -------------------
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" + apiKey +
                 "&field1=" + String(temperature) +
                 "&field2=" + String(humidity) +
                 "&field3=" + String(alarmActive ? 1 : 0) +
                 "&field4=" + String(doorOpen ? 1 : 0);

    http.begin(url);
    http.GET();
    http.end();
  }

  Serial.print("Temp:");
  Serial.print(temperature);
  Serial.print(" Hum:");
  Serial.print(humidity);
  Serial.print(" DoorADC:");
  Serial.println(doorValue);

  delay(2000);
}
