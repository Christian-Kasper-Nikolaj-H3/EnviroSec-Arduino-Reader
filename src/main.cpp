#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Pin definitions
#define SS_PIN 10
#define RST_PIN 9
#define RED_PIN 6
#define BLUE_PIN 3
#define GREEN_PIN 5

// LED timing
#define LED_DURATION 5000
#define RESPONSE_TIMEOUT 5000

// Device identifier
auto DEVICE_ID = "1";

// LED brightness
constexpr float BRIGHTNESS = 0.3;

// MQTT configuration
constexpr int MQTT_PORT = 1883;
auto MQTT_TOPIC_REQUEST = "access/request";
auto MQTT_TOPIC_RESPONSE = "access/response/1";

// Objects
MFRC522 rfid(SS_PIN, RST_PIN);
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// LED state
unsigned long ledOnTime = 0;
bool ledActive = false;
bool awaitingResponse = false;
unsigned long requestSentTime = 0;

void setColor(const float red, const float green, const float blue) {
    analogWrite(RED_PIN, static_cast<int>(red * BRIGHTNESS));
    analogWrite(GREEN_PIN, static_cast<int>(green * BRIGHTNESS));
    analogWrite(BLUE_PIN, static_cast<int>(blue * BRIGHTNESS));
}

void turnOffLed() {
    setColor(0, 0, 0);
    ledOnTime = 0;
    ledActive = false;
}

void showWaiting() {
    Serial.println("Waiting for response...");
    turnOffLed();
    setColor(255, 255, 0);
    ledOnTime = millis();
    ledActive = true;
}

void showStatus(const int statusCode) {
    turnOffLed();
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

void showTimeout() {
    Serial.println("Request timed out");
    turnOffLed();
    setColor(255, 165, 0);
    ledOnTime = millis();
    ledActive = true;
    awaitingResponse = false;
}

void connectToWifi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setColor(0,0,255);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected! IP: ");
    delay(1000);
    Serial.println(WiFi.localIP());
    turnOffLed();
}

void handleMQTTMessage(const int messageSize) {
    if (messageSize == 0) return;

    String message = "";
    while (mqttClient.available()) {
        message += static_cast<char>(mqttClient.read());
    }

    Serial.print("Received response: ");
    Serial.println(message);

    // Parse response JSON
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        showStatus(500);
        return;
    }

    const int statusCode = doc["status"] | 500;
    showStatus(statusCode);
}

// Add this callback function before connectMQTT()
void onMqttMessage(const int messageSize) {
    Serial.println(">>> CALLBACK: Message received! <<<");
    Serial.print("Topic: ");
    Serial.println(mqttClient.messageTopic());
    Serial.print("Size: ");
    Serial.println(messageSize);

    // Read and handle the message
    handleMQTTMessage(messageSize);
}

void connectMQTT() {
    Serial.print("Connecting to MQTT broker...");
    setColor(0, 255, 255);

    // Set a unique client ID
    mqttClient.setId("arduino-access-panel-1");

    // Set keep-alive to 15 seconds (more responsive)
    mqttClient.setKeepAliveInterval(15 * 1000);

    // Set message callback
    mqttClient.onMessage(onMqttMessage);

    while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.print(".");
        delay(500);
    }

    Serial.println(" connected!");

    // Subscribe with QoS 1
    Serial.print("Subscribing to: ");
    Serial.println(MQTT_TOPIC_RESPONSE);

    const int subscribeResult = mqttClient.subscribe(MQTT_TOPIC_RESPONSE, 1);
    Serial.print("Subscribe result: ");
    Serial.println(subscribeResult);

    turnOffLed();
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

    // Show the waiting state
    awaitingResponse = true;
    requestSentTime = millis();
    showWaiting();
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
        Serial.println("MQTT disconnected! Reconnecting...");
        connectMQTT();
    }

    // Poll for messages - callback handles incoming messages
    mqttClient.poll();

    // Check for response timeout
    if (awaitingResponse && millis() - requestSentTime >= RESPONSE_TIMEOUT) {
        Serial.println("Request timed out");
        showTimeout();
    }

    // Check if the LED timer has expired
    if (ledActive && !awaitingResponse && millis() - ledOnTime > LED_DURATION) {
        turnOffLed();
        Serial.println("LED off");
    }

    // Don't scan new cards while busy
    if (awaitingResponse || ledActive) return;

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