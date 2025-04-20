#include <SPI.h>
#include <MFRC522.h>

// Pin definitions
#define RST_PIN 22
#define SS_PIN 21
#define GREEN_LED 13
#define RED_LED 12
#define BUZZER 14

// Create MFRC522 instance
MFRC522 rfid(SS_PIN, RST_PIN);

// Authorized card UID (replace with your card's UID)
byte authorizedUID[] = {0x12, 0x34, 0x56, 0x78}; // Example UID, update this!

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize RFID reader
  rfid.PCD_Init();
  Serial.println("RFID Reader Initialized. Scan a card...");

  // Set pin modes
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  // Turn off LEDs and buzzer initially
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  // Check for a new card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Read the card's UID
  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Check if the UID matches the authorized one
  bool accessGranted = true;
  for (byte i = 0; i < 4; i++) { // Assuming 4-byte UID
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      accessGranted = false;
      break;
    }
  }

  if (accessGranted) {
    // Access Allowed: Turn on green LED
    Serial.println("Access Allowed!");
    digitalWrite(GREEN_LED, HIGH);
    delay(2000); // Keep LED on for 2 seconds
    digitalWrite(GREEN_LED, LOW);
  } else {
    // Access Denied: Turn on red LED and buzzer
    Serial.println("Access Denied!");
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(1000); // Keep LED and buzzer on for 1 second
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
  }

  // Halt PICC and stop encryption
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}