#include <SPI.h>  
#include <Wire.h>  
#include <Adafruit_GFX.h>  
#include <Adafruit_SSD1306.h>  
#include <MFRC522.h>  

// OLED display settings  
#define SCREEN_WIDTH 128  
#define SCREEN_HEIGHT 64  
#define OLED_RESET -1  
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  

// RFID module pins  
#define SS_PIN 2  
#define RST_PIN 5  

// Components  
#define GREEN_LED 12  
#define RED_LED 14  
#define BUZZER_PIN 27  

// RFID & storage  
MFRC522 rfid(SS_PIN, RST_PIN);  
#define MAX_UIDS 10  
byte authorizedUIDs[MAX_UIDS][4]; // Array to hold up to 10 UIDs  
int authorizedCount = 0;  
String serialInput = "";  

// Define sector and block for storing variable
#define SECTOR 1
#define BLOCK 4 // Block 4 in sector 1 (avoid using block 7, which is the sector trailer)
byte keyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Default key for MIFARE Classic

void setup()  
{  
    Serial.begin(115200);  
    SPI.begin();  
    rfid.PCD_Init();  

    pinMode(GREEN_LED, OUTPUT);  
    pinMode(RED_LED, OUTPUT);  
    pinMode(BUZZER_PIN, OUTPUT);  

    digitalWrite(GREEN_LED, LOW);  
    digitalWrite(RED_LED, LOW);  
    digitalWrite(BUZZER_PIN, LOW);  

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))  
    {  
        Serial.println("SSD1306 initialization failed!");  
        while (true);  
    }  

    PrintReadyMessage();  
    Serial.println("System Ready. Use 'add', 'list', 'clear', 'write <value>', or 'read' commands in Serial Monitor.");  
}  

void loop()  
{  
    // Handle Serial Monitor Commands  
    if (Serial.available())  
    {  
        serialInput = Serial.readStringUntil('\n');  
        serialInput.trim();  
        handleSerialCommand(serialInput);  
    }  

    // RFID Detection  
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())  
        return;  

    bool isAuthorized = checkUID(rfid.uid.uidByte);  

    if (isAuthorized)  
    {  
        showMessage("Access Granted");  
        digitalWrite(GREEN_LED, HIGH);  
        beep(1, 200);  
        // Read and display stored variable  
        String storedValue = readVariableFromCard();  
        if (storedValue != "") {  
            showMessage("Value: " + storedValue);  
            delay(2000);  
        }  
    }  
    else  
    {  
        showMessage("Access Denied");  
        digitalWrite(RED_LED, HIGH);  
        beep(2, 400);  
    }  

    // Print UID to Serial Monitor  
    printUID();  

    delay(2000);  
    resetIndicators();  
    rfid.PICC_HaltA();  
    PrintReadyMessage();  
}  

void beep(int times, int duration)  
{  
    for (int i = 0; i < times; i++)  
    {  
        digitalWrite(BUZZER_PIN, HIGH);  
        delay(duration);  
        digitalWrite(BUZZER_PIN, LOW);  
        delay(100);  
    }  
}  

void resetIndicators()  
{  
    digitalWrite(GREEN_LED, LOW);  
    digitalWrite(RED_LED, LOW);  
    digitalWrite(BUZZER_PIN, LOW);  
}  

void printUID()  
{  
    Serial.print("UID: ");  
    for (byte i = 0; i < rfid.uid.size; i++)  
    {  
        Serial.print(rfid.uid.uidByte[i], HEX);  
        Serial.print(" ");  
    }  
    Serial.println();  
}  

void showMessage(String msg)  
{  
    display.clearDisplay();  
    display.setTextSize(1);  
    display.setTextColor(WHITE);  
    display.setCursor(10, 20);  
    display.println(msg);  
    display.display();  
    Serial.println(msg);  
}  

void PrintReadyMessage()  
{  
    display.clearDisplay();  
    display.setTextSize(1);  
    display.setTextColor(WHITE);  
    display.setCursor(10, 10);  
    display.println("ESP32 RFID Ready...");  
    display.display();  
}  

bool compareUIDs(byte *uid1, byte *uid2)  
{  
    for (int i = 0; i < 4; i++)  
    {  
        if (uid1[i] != uid2[i])  
            return false;  
    }  
    return true;  
}  

bool checkUID(byte *uid)  
{  
    for (int i = 0; i < authorizedCount; i++)  
    {  
        if (compareUIDs(authorizedUIDs[i], uid))  
            return true;  
    }  
    return false;  
}  

void addCurrentUID()  
{  
    if (authorizedCount >= MAX_UIDS)  
    {  
        Serial.println("UID storage full!");  
        return;  
    }  

    // Check if card is scanned  
    if (rfid.uid.uidByte[0] == 0)  
    {  
        Serial.println("Please scan a card before using 'add'.");  
        return;  
    }  

    for (int i = 0; i < authorizedCount; i++)  
    {  
        if (compareUIDs(authorizedUIDs[i], rfid.uid.uidByte))  
        {  
            Serial.println("UID already in list.");  
            return;  
        }  
    }  

    // Add UID  
    for (int i = 0; i < 4; i++)  
    {  
        authorizedUIDs[authorizedCount][i] = rfid.uid.uidByte[i];  
    }  
    authorizedCount++;  
    Serial.println("UID added successfully.");  
}  

bool authenticateCard() {  
    MFRC522::MIFARE_Key key;  
    for (byte i = 0; i < 6; i++) {  
        key.keyByte[i] = keyA[i];  
    }  
    MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BLOCK, &key, &(rfid.uid));  
    if (status != MFRC522::STATUS_OK) {  
        Serial.print("Authentication failed: ");  
        Serial.println(rfid.GetStatusCodeName(status));  
        return false;  
    }  
    return true;  
}  

bool writeVariableToCard(String value) {  
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {  
        Serial.println("No card detected for writing.");  
        return false;  
    }  

    if (!authenticateCard()) {  
        rfid.PICC_HaltA();  
        return false;  
    }  

    byte buffer[16];  
    memset(buffer, 0, 16); // Clear buffer  
    value.getBytes(buffer, 16); // Copy string to buffer (max 16 bytes)  

    MFRC522::StatusCode status = rfid.MIFARE_Write(BLOCK, buffer, 16);  
    if (status != MFRC522::STATUS_OK) {  
        Serial.print("Write failed: ");  
        Serial.println(rfid.GetStatusCodeName(status));  
        rfid.PICC_HaltA();  
        return false;  
    }  

    Serial.println("Variable written to card: " + value);  
    rfid.PICC_HaltA();  
    return true;  
}  

String readVariableFromCard() {  
    if (!authenticateCard()) {  
        rfid.PICC_HaltA();  
        return "";  
    }  

    byte buffer[18]; // Buffer to hold data (16 bytes + 2 bytes CRC)  
    byte size = 18;  

    MFRC522::StatusCode status = rfid.MIFARE_Read(BLOCK, buffer, &size);  
    if (status != MFRC522::STATUS_OK) {  
        Serial.print("Read failed: ");  
        Serial.println(rfid.GetStatusCodeName(status));  
        rfid.PICC_HaltA();  
        return "";  
    }  

    // Convert buffer to string, stopping at null or max 16 bytes  
    char value[17];  
    memset(value, 0, 17);  
    for (byte i = 0; i < 16 && buffer[i] != 0; i++) {  
        value[i] = (char)buffer[i];  
    }  

    rfid.PICC_HaltA();  
    return String(value);  
}  

void handleSerialCommand(String cmd)  
{  
    if (cmd == "list")  
    {  
        Serial.println("Authorized UIDs:");  
        for (int i = 0; i < authorizedCount; i++)  
        {  
            for (int j = 0; j < 4; j++)  
            {  
                Serial.print(authorizedUIDs[i][j], HEX);  
                Serial.print(" ");  
            }  
            Serial.println();  
        }  
        if (authorizedCount == 0)  
            Serial.println("No UIDs stored.");  
    }  
    else if (cmd == "add")  
    {  
        addCurrentUID();  
    }  
    else if (cmd == "clear")  
    {  
        authorizedCount = 0;  
        Serial.println("All UIDs cleared.");  
    }  
    else if (cmd.startsWith("write "))  
    {  
        String value = cmd.substring(6);  
        value.trim();  
        if (value.length() > 16) {  
            Serial.println("Value too long (max 16 characters).");  
            return;  
        }  
        if (value.length() == 0) {  
            Serial.println("Please provide a value to write.");  
            return;  
        }  
        if (writeVariableToCard(value)) {  
            showMessage("Write Success");  
        } else {  
            showMessage("Write Failed");  
        }  
    }  
    else if (cmd == "read")  
    {  
        String value = readVariableFromCard();  
        if (value != "") {  
            Serial.println("Stored value: " + value);  
            showMessage("Value: " + value);  
        } else {  
            Serial.println("Failed to read value.");  
            showMessage("Read Failed");  
        }  
    }  
    else  
    {  
        Serial.println("Unknown command. Use: add / list / clear / write <value> / read");  
    }  
}