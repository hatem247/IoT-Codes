#include <WiFi.h>
#include <Firebase.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <UbidotsESPMQTT.h>

#define TOKEN "BBUS-byrMkHkt4r6DV55zMBaseWa4NlbEtY"
#define DEVICE_LABEL "attendance-device"
#define ATTENDANCE_VARIABLE_LABEL "attendance"
#define LOGS_VARIABLE_LABEL "logs"
#define UID_VARIABLE_LABEL "uid"
Ubidots client(TOKEN);

// WiFi credentials
#define WIFI_SSID "Hatem's S21 ultra"
#define WIFI_PASSWORD "Hatem_24"

// Firebase credentials
#define FIREBASE_TOKEN "dqizmZeCvJxtznOab1n10qyZo8oa5NgIFzkSdpvF"
#define FIREBASE_URL "https://student-attendance-syste-8f1a5-default-rtdb.firebaseio.com/"
Firebase fb(FIREBASE_URL, FIREBASE_TOKEN);

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// RFID setup
#define SS_PIN 2
#define RST_PIN 5
#define GREEN_LED 12
#define RED_LED 14
#define BUZZER_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

// Lecture start time (10:00 AM)
const int lectureStartHour = 10;
const int lectureStartMinute = 0;
void callback(char* topic, byte* payload, unsigned int length);
void connectToWiFi();
void configureTime();
void showMessage(String msg);
String getUIDAsNumber(byte *uid, byte size);
bool isAuthorized(String uid);
void recordAttendance(String uid, String attendanceTime, int lateMinutes);
void logScan(String uid, bool authorized);
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
        Serial.println("OLED init failed!");
        while (true);
    }

    connectToWiFi();
    configureTime();

    client.setDebug(true);
    client.wifiConnection(WIFI_SSID, WIFI_PASSWORD);
    client.begin(callback);

    resetToReadyMessage();
}

void loop() {
    if (!client.connected()) {
      client.reconnect();
    }
    client.loop();

    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }

    String uidString = getUIDAsNumber(rfid.uid.uidByte, rfid.uid.size);
    Serial.println("Scanned UID: " + uidString);

    // Check if the UID is authorized
    bool authorized = isAuthorized(uidString);

    // Calculate time and lateness
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    String attendanceTime = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min);
    int lateMinutes = (timeinfo.tm_hour - lectureStartHour) * 60 + (timeinfo.tm_min - lectureStartMinute);

    // Store attendance or log unauthorized access
    if (authorized) {
        logScan(uidString, true);
        recordAttendance(uidString, attendanceTime, lateMinutes);
        showMessage("Access Granted");
        Serial.println("Access Granted");
        digitalWrite(GREEN_LED, HIGH);
        beep(1, 200);
        client.add(LOGS_VARIABLE_LABEL, 1);
        client.add(ATTENDANCE_VARIABLE_LABEL, 1);
        client.add(UID_VARIABLE_LABEL, uidString.toDouble());
        client.ubidotsPublish(DEVICE_LABEL);
    } else {
        logScan(uidString, false);
        showMessage("Access Denied");
        Serial.println("Access Denied");
        digitalWrite(RED_LED, HIGH);
        beep(2, 400);
        client.add(LOGS_VARIABLE_LABEL, 1);
        client.add(ATTENDANCE_VARIABLE_LABEL, 0);
        client.add(UID_VARIABLE_LABEL, uidString.toDouble());
        client.ubidotsPublish(DEVICE_LABEL);
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
        Serial.print(".");
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.display();
    Serial.println("\nWi-Fi connected!");
}

void configureTime() {
    // Set timezone to UTC+2 for Egypt/Giza and use reliable NTP servers
    configTime(7200, 0, "pool.ntp.org", "time.nist.gov"); // 7200 seconds = 2 hours (UTC+2)
    Serial.println("Waiting for time synchronization...");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
    } else {
        Serial.println(&timeinfo, "Time synchronized: %A, %B %d %Y %H:%M:%S");
    }
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
    String path = "authorized/" + uid;
    int result = fb.getInt(path);
    return (result == 1);
}

void recordAttendance(String uid, String attendanceTime, int lateMinutes) {
    String attendanceRecord = attendanceTime + ", " + String(lateMinutes);
    String path = "attendance/" + uid;
    fb.setString(path, attendanceRecord);
    
}

void logScan(String uid, bool authorized) {
    String status = authorized ? "Authorized" : "Unauthorized";
    String logPath = "logs/" + String(millis());
    fb.setString(logPath, "UID: " + uid + ", Status: " + status);
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("===== Incoming MQTT Message =====");
  Serial.print("Topic: ");
  Serial.println(topic);
  String message = "";
  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println("=================================\n");
}
