#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>

#define WIFI_SSID "wifi"
#define WIFI_PASSWORD "password"

#define FIREBASE_HOST "https://temp-reader-iot-default-rtdb.firebaseio.com/authorized"
#define FIREBASE_TOKEN "AIzaSyAHDEox5BGGff-Loi-tmE2tG-zqBTgVDvQ"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SS_PIN 2
#define RST_PIN 5
#define GREEN_LED 12
#define RED_LED 14
#define BUZZER_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

void connectToWiFi();
void showMessage(String msg);
String getUIDAsNumber(byte *uid, byte size);
bool isAuthorized(String uid);
void beep(int times, int duration);
void resetIndicators();
void resetToReadyMessage();

void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();

    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (true);
    }

    connectToWiFi();
    resetToReadyMessage();
}

void loop() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }

    String uidString = getUIDAsNumber(rfid.uid.uidByte, rfid.uid.size);
    Serial.println("Scanned UID: " + uidString);

    if (isAuthorized(uidString)) {
        showMessage("Access Granted");
        Serial.println("Access Granted");
        digitalWrite(GREEN_LED, HIGH);
        beep(1, 200);
    } else {
        showMessage("Access Denied");
        Serial.println("Access Denied");
        digitalWrite(RED_LED, HIGH);
        beep(2, 400);
    }

    delay(3000);
    resetIndicators();
    resetToReadyMessage();
    rfid.PICC_HaltA();
}

void connectToWiFi() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Connecting to WiFi...");
    display.display();
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.display();
    Serial.println("WiFi Connected!");
    delay(2000);
}

void showMessage(String msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    display.println(msg);
    display.display();
}

String getUIDAsNumber(byte *uid, byte size) {
    String uidString = "";
    for (byte i = 0; i < size; i++) {
        uidString += String(uid[i]);
    }
    return uidString;
}

bool isAuthorized(String uid) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(FIREBASE_HOST) + "/" + uid + ".json?auth=" + FIREBASE_TOKEN;
        Serial.println("Requesting URL: " + url);
        http.begin(url);

        int httpCode = http.GET();
        bool authorized = false;

        if (httpCode == 200) {
            String payload = http.getString();
            Serial.println("Firebase Response: " + payload);
            if(payload == "1"){
                authorized = payload == "1";
            }
        } else {
            Serial.println("HTTP Error Code: " + String(httpCode));
        }

        http.end();
        return authorized;
    } else {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("WiFi not connected");
        display.display();
        Serial.println("WiFi not connected");
        delay(2000);
        return false;
    }
}

void beep(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
    }
}

void resetIndicators() {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

void resetToReadyMessage() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 10);
    display.println("ESP32 RFID Ready...");
    display.display();
}