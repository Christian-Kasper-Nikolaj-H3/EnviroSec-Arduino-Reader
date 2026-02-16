#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

// Pin definitions
#define SS_PIN 10
#define RST_PIN 9
#define RED_PIN 6
#define BLUE_PIN 5
#define GREEN_PIN 3

// LED timing
#define LED_DURATION 5000

// Device identifier
const char* DEVICE_ID = "device1";

//WiFi credentials
const char* WIFI_SSID = "Lab-ZBC";
const char* WIFI_PASSWORD = "Prestige#PuzzledCASH48!";

// MQTT configuration
const char* MQTT_BROKER = "raspberry_pi_ip_address";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC_REQUEST = "access/request";
const char* MQTT_TOPIC_RESPONSE = "access/response/1";

// Objects
MFRC522 rfid(SS_PIN, RST_PIN);
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// LED state
unsigned long ledOnTime = 0;
bool ledActive = false;
bool awaitingResponse = false;

void setColor(int red, int green, int blue) {
    analogWrite(RED_PIN, red);
    analogWrite(GREEN_PIN, green);
    analogWrite(BLUE_PIN, blue);
}

void turnOffLed() {
    setColor(0, 0, 0);
    ledActive = false;
}

void showStatus(int statusCode) {
    if (statusCode == 200) {
        // Green for access granted
        Serial.println("Access GRANTED");
        setColor(0, 255, 0);
    } else if (statusCode == 403) {
        // Red for access denied
        Serial.println("Access DENIED");
        setColor(255, 0, 0);
    } else {
        // Orange for errors (404 or other)
        Serial.println("Error occurred");
        setColor(255, 165, 0);
    }
    ledOnTime = millis();
    ledActive = true;
    awaitingResponse = false;
}

void connectToWifi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
}

void connectMQTT() {
    Serial.print("Connecting to MQTT broker...");

    while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.print(".");
        delay(500);
    }

    Serial.println(" connected!");

    // Subscribe to device-specific response topic
    mqttClient.subscribe(MQTT_TOPIC_RESPONSE);
    Serial.print("Subscribed to: ");
    Serial.println(MQTT_TOPIC_RESPONSE);
}

void sendAccessRequest(const String& rfidTag) {
    // Create JSON payload
    JsonDocument doc;
    doc["rfid"] = rfidTag;
    doc["device"] = DEVICE_ID;

    String payload;
    serializeJson(doc, payload);

    // Publish to MQTT
    mqttClient.beginMessage(MQTT_TOPIC_REQUEST);
    mqttClient.print(payload);
    mqttClient.endMessage();

    Serial.print("Sent request: ");
    Serial.println(payload);

    awaitingResponse = true;
}

void handleMQTTMessage(int messageSize) {
    if (messageSize == 0) return;

    String message = "";
    while (mqttClient.available()) {
        message += (char)mqttClient.read();
    }

    Serial.print("Received response: ");
    Serial.println(message);

    // Parse response JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        showStatus(500);
        return;
    }

    int statusCode = doc["status"] | 500;
    showStatus(statusCode);
}

void setup() {
    Serial.begin(9600);

    // Initialize LED pins
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    turnOffLed();

    // Initialize SPI and RFID
    SPI.begin();
    rfid.PCD_Init();

    // Connect to network
    connectToWifi();
    connectMQTT();

    Serial.println("Access panel ready. Waiting for card...");
}

void loop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.poll();

    // Check for incoming MQTT messages
    int messageSize = mqttClient.parseMessage();
    if (messageSize) {
        handleMQTTMessage(messageSize);
    }

    // Check if LED timer has expired
    if (ledActive && millis() - ledOnTime > LED_DURATION) {
        turnOffLed();
        Serial.println("LED off");
    }

    // Don't scan new cards while awaiting response
    if (awaitingResponse) return;

    // Check for RFID card
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    // Build RFID string
    String strID = "";
    for (byte i = 0; i < 4; i++) {
        strID +=
            (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
                String(rfid.uid.uidByte[i], HEX) +
                    (i != 3 ? ":" : "");
    }
    strID.toUpperCase();

    Serial.print("RFID tag detected: ");
    Serial.println(strID);

    // Send access request to gateway
    sendAccessRequest(strID);

    // Halt PICC
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}