#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>

#define WIFI_SSID "ssid1"
#define WIFI_PASSWORD "password"
#define FIREBASE_HOST "https://iotrtdb-default-rtdb.firebaseio.com/data"
#define FIREBASE_TOKEN "0Iph3bw2F1Q3rtO9TlIVGiZSvxDK7OEfYURlLLYV"
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
#define MAX_UIDS 10
byte authorizedUIDs[MAX_UIDS][4];
int authorizedCount = 0;

void connectToWiFi();
void displayTemperatureUIDMessage(String uid);
void updateFirebaseUID(String uid);
String getUIDString(byte *uid, byte size);
void beep(int times, int duration);
void resetIndicators();
void showMessage(String msg);
void PrintReadyMessage();
bool compareUIDs(byte *uid1, byte *uid2);
bool checkUID(byte *uid);
void addCurrentUID();
void handleSerialCommand(String cmd);

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
    PrintReadyMessage();
}

void loop() {
    if (Serial.available()) {
        String serialInput = Serial.readStringUntil('\n');
        serialInput.trim();
        handleSerialCommand(serialInput);
    }
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }
    String uidString = getUIDString(rfid.uid.uidByte, rfid.uid.size);
    bool isAuthorized = checkUID(rfid.uid.uidByte);
    if (isAuthorized) {
        updateFirebaseUID(uidString);
        showMessage("Access Granted");
        Serial.println("Access Granted");
        digitalWrite(GREEN_LED, HIGH);
        beep(1, 200);
        Serial.println(uidString);
        displayTemperatureUIDMessage(uidString);
        delay(5000);
    } else {
        showMessage("Access Denied");
        Serial.println("Access Denied");
        Serial.println(uidString);
        digitalWrite(RED_LED, HIGH);
        beep(2, 400);
    }
    delay(2000);
    resetIndicators();
    rfid.PICC_HaltA();
    PrintReadyMessage();
}

void connectToWiFi() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Connecting WiFi");
    Serial.print("Connecting WiFi: ");
    Serial.println(WIFI_SSID);
    display.display();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    Serial.println("WiFi Connected!");
    display.display();
    delay(1000);
}

void displayTemperatureUIDMessage(String uid) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(FIREBASE_HOST) + "/temp.json?auth=" + FIREBASE_TOKEN;
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            int temp = payload.toInt();
            String message;
            if (temp > 40) {
                message = "Overheat...yaa7";
                tone(BUZZER_PIN, 1000, 500);
            } else if (temp < 20) {
                message = "Cooooollldddd";
                tone(BUZZER_PIN, 1000, 500);
            } else {
                message = "Good weather";
                noTone(BUZZER_PIN);
            }
            Serial.println("UID: " + uid);
            Serial.println("Temp: " + String(temp) + " C");
            Serial.println("Message: " + message);
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("UID: " + uid);
            display.setCursor(0, 20);
            display.print("Temp: ");
            display.print(temp);
            display.println(" C");
            display.setCursor(0, 40);
            display.println("Message: " + message);
            display.display();
        } else {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Firebase error");
            display.display();
        }
        http.end();
    }
}

void updateFirebaseUID(String uid) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(FIREBASE_HOST) + "/UID.json?auth=" + FIREBASE_TOKEN;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        String jsonPayload = "\"" + uid + "\"";
        http.PUT(jsonPayload);
        http.end();
    }
}

String getUIDString(byte *uid, byte size) {
    String uidString = "";
    for (byte i = 0; i < size; i++) {
        uidString += String(uid[i], HEX);
        if (i < size - 1) {
            uidString += ":";
        }
    }
    return uidString;
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

void showMessage(String msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    display.println(msg);
    display.display();
}

void PrintReadyMessage() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 10);
    display.println("ESP32 RFID Ready...");
    display.display();
}

bool compareUIDs(byte *uid1, byte *uid2) {
    for (int i = 0; i < 4; i++) {
        if (uid1[i] != uid2[i]) {
            return false;
        }
    }
    return true;
}

bool checkUID(byte *uid) {
    for (int i = 0; i < authorizedCount; i++) {
        if (compareUIDs(authorizedUIDs[i], uid)) {
            return true;
        }
    }
    return false;
}

void addCurrentUID() {
    if (authorizedCount >= MAX_UIDS) {
        return;
    }
    for (int i = 0; i < authorizedCount; i++) {
        if (compareUIDs(authorizedUIDs[i], rfid.uid.uidByte)) {
            return;
        }
    }
    for (int i = 0; i < 4; i++) {
        authorizedUIDs[authorizedCount][i] = rfid.uid.uidByte[i];
    }
    authorizedCount++;
}

void handleSerialCommand(String cmd) {
    if (cmd == "list") {
        for (int i = 0; i < authorizedCount; i++) {
            for (int j = 0; j < 4; j++) {
                Serial.print(authorizedUIDs[i][j], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    } else if (cmd == "add") {
        addCurrentUID();
        Serial.println("UID Added Successfully");
    } else if (cmd == "clear") {
        authorizedCount = 0;
        Serial.println("All UID's Cleared");
    }
}