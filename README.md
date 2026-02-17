# EnviroSec Arduino - RFID Access Panel

An Arduino-based RFID access control system that communicates with a central gateway via MQTT. This device simulates a door lock/access panel that validates access cards through a backend API.

## 🔧 Hardware Requirements

| Component           | Description                               |
|---------------------|-------------------------------------------|
| Arduino UNO R4 WiFi | Main microcontroller with WiFi capability |
| MFRC522 (RC522)     | RFID reader module                        |
| KY-016              | RGB LED module                            |
| Jumper wires        | For connections                           |

## 📌 Wiring Diagram

### RFID-RC522 Module

| RC522 Pin | Arduino Pin |
|-----------|-------------|
| SDA (SS)  | 10          |
| SCK       | 13          |
| MOSI      | 11          |
| MISO      | 12          |
| RST       | 9           |
| 3.3V      | 3.3V        |
| GND       | GND         |

### KY-016 RGB LED Module

| LED Pin | Arduino Pin |
|---------|-------------|
| R       | 6           |
| G       | 3           |
| B       | 5           |
| GND     | GND         |

## 🚀 Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) or Arduino IDE
- Access to the EnviroSec gateway (Raspberry Pi)

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/Christian-Kasper-Nikolaj-H3/EnviroSec-Arduino-Reader.git
   cd EnviroSec-Arduino-Reader
   ```

2. **Create your secrets file**
   ```bash
   cp src/secrets.example.h src/secrets.h
   ```

3. **Edit `src/secrets.h`** with your credentials:
   ```cpp
   const char* WIFI_SSID = "YourWiFiName";
   const char* WIFI_PASSWORD = "YourWiFiPassword";
   const char* MQTT_BROKER = "192.168.1.100"; // Your Raspberry Pi IP
   ```

4. **Install dependencies** (PlatformIO handles this automatically, or install via Arduino Library Manager):
    - MFRC522
    - ArduinoMqttClient
    - ArduinoJson

5. **Upload to Arduino**
   ```bash
   # PlatformIO
   pio run -t upload

   # Or use Arduino IDE / CLion upload button
   ```

## 📡 MQTT Communication

### Topics

| Topic                         | Direction         | Description              |
|-------------------------------|-------------------|--------------------------|
| `access/request`              | Arduino → Gateway | Access request with RFID |
| `access/response/{device_id}` | Gateway → Arduino | Access decision          |

### Request Payload