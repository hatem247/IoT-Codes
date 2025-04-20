#include <WiFi.h>
#include <UbidotsESPMQTT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TOKEN "BBUS-23oFDObrNpMmsBXAdUeKVk23y3N2BL"
#define WIFINAME "wifi"
#define WIFIPASS "password"

#define DEVICE_LABEL "esp32"
#define TEMP_VARIABLE_LABEL "temp"
#define LED_VARIABLE_LABEL "led"

#define LED_PIN 12
#define PUBLISH_INTERVAL 5000

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Ubidots client(TOKEN);
int ledState = 0;

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
  Serial.println();
  int value = message.toInt();
  if (value == 1) {
    ledState = 1;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED turned ON via Ubidots");
  } else {
    ledState = 0;
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED turned OFF via Ubidots");
  }
  displayStatus(0.0, ledState);
  Serial.println("=================================\n");
}

void displayStatus(float temperature, int ledState) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 Status:");
  display.setCursor(0, 16);
  display.print("Temperature: ");
  display.print(temperature);
  display.println(" C");
  display.setCursor(0, 32);
  display.print("LED State: ");
  display.println(ledState ? "ON" : "OFF");
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFINAME);
  WiFi.begin(WIFINAME, WIFIPASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
  client.setDebug(true);
  client.wifiConnection(WIFINAME, WIFIPASS);
  client.begin(callback);
  client.ubidotsSubscribe(DEVICE_LABEL, LED_VARIABLE_LABEL);
}

void loop() {
  if (!client.connected()) {
    client.reconnect();
    client.ubidotsSubscribe(DEVICE_LABEL, LED_VARIABLE_LABEL);
  }
  float temperature = 24.5 + random(-10, 10) / 10.0;
  Serial.println("Publishing Sensor Data...");
  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("LED State: ");
  Serial.println(ledState);
  client.add(TEMP_VARIABLE_LABEL, temperature);
  client.ubidotsPublish(DEVICE_LABEL);
  displayStatus(temperature, ledState);
  client.loop();
  delay(PUBLISH_INTERVAL);
}