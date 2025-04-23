#include <WiFi.h>
#include <FirebaseESP32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// WiFi credentials
#define WIFI_SSID "Hatem's S21 ultra"
#define WIFI_PASSWORD "Hatem_12"

// Firebase credentials
#define FIREBASE_HOST "https://student-attendance-syste-8f1a5-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "dqizmZeCvJxtznOab1n10qyZo8oa5NgIFzkSdpvF"

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

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

// Lecture start time (10:00 AM)
const int lectureStartHour = 10;
const int lectureStartMinute = 0;

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

    // Firebase setup
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);

    resetToReadyMessage();
}

void loop() {
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
        recordAttendance(uidString, attendanceTime, lateMinutes);
        showMessage("Access Granted");
        Serial.println("Access Granted");
        digitalWrite(GREEN_LED, HIGH);
        beep(1, 200);
    } else {
        logScan(uidString, authorized);
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
    if (Firebase.getInt(firebaseData, "/authorized/" + uid)) {
        return firebaseData.intData() == 1;
    } else {
        Serial.println("Error checking authorization: " + firebaseData.errorReason());
        return false;
    }
}

void recordAttendance(String uid, String attendanceTime, int lateMinutes) {
    String attendanceRecord = attendanceTime + ", " + String(lateMinutes);
    if (Firebase.setString(firebaseData, "/attendance/" + uid, attendanceRecord)) {
        Serial.println("Attendance recorded for UID: " + uid);
    } else {
        Serial.println("Error recording attendance: " + firebaseData.errorReason());
    }
}

void logScan(String uid, bool authorized) {
    String status = authorized ? "Authorized" : "Unauthorized";
    String logPath = "/logs/" + String(millis());
    if (Firebase.setString(firebaseData, logPath, "UID: " + uid + ", Status: " + status)) {
        Serial.println("Log recorded: " + status + " UID: " + uid);
    } else {
        Serial.println("Error logging scan: " + firebaseData.errorReason());
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