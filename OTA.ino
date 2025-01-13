#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>

// WiFi credentials
const char* ssid = "iPhone";
const char* password = "12345678";

// MQTT credentials
const char* mqttServer = "thingsboard.cloud";
const int mqttPort = 1883;
const char* mqttUsername = "9ofsuj1w6mb985zx5sdv"; // Replace with your device's access token

// Topics
const char* otaTopic = "v1/devices/me/attributes";
const char* telemetryTopic = "v1/devices/me/telemetry";

// Predefined OTA URL (Direct link to firmware binary)
const char* predefinedOTAUrl = "https://drive.google.com/uc?export=download&id=1vTRHbXDwK1LHnUHC6tJljJAL6-pTTfWm";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool isFirmwareUpgradeTriggered = false;

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
}

void connectToMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqttUsername, "")) {
      Serial.println("Connected to MQTT!");
      mqttClient.subscribe(otaTopic);
      mqttClient.subscribe(telemetryTopic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void handleOTAUpdate(const char* firmwareUrl) {
  HTTPClient http;
  http.begin(firmwareUrl);  // ใช้ URL ที่ได้รับจาก MQTT payload
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // ติดตามการเปลี่ยนเส้นทาง (HTTP redirects)
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    size_t contentLength = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(*stream);

      if (written == contentLength) {
        Serial.println("Firmware update complete. Rebooting...");
        Update.end();
        ESP.restart();
      } else {
        Serial.println("Firmware update failed. Aborting...");
        Update.abort();
      }
    } else {
      Serial.println("Not enough space for update. Aborting...");
    }
  } else {
    Serial.printf("Failed to fetch firmware: HTTP code %d\n", httpCode);
  }
  http.end();
}

void processTelemetry(const char* payload) {
  Serial.println("Processing telemetry...");
  Serial.println(payload);
  // Add logic to handle telemetry here
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate the payload
  Serial.printf("Message arrived on topic: %s\n", topic);
  Serial.printf("Payload: %s\n", (char*)payload);

  if (strcmp(topic, otaTopic) == 0) {
    if (!isFirmwareUpgradeTriggered) {
      isFirmwareUpgradeTriggered = true;
      Serial.println("Firmware upgrade triggered!");
      handleOTAUpdate(predefinedOTAUrl); // Use predefined URL from the message
    }
  } else if (strcmp(topic, telemetryTopic) == 0) {
    processTelemetry((char*)payload);
  }
}

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  mqttClient.setCallback(callback);
  connectToMQTT();

  ArduinoOTA.setPort(3232);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update started...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Update completed!");
    isFirmwareUpgradeTriggered = false;
  });
  ArduinoOTA.begin();

  pinMode(2, OUTPUT);
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();
  ArduinoOTA.handle();

  // Blink LED for testing
  digitalWrite(2, HIGH);
  delay(50);
  digitalWrite(2, LOW);
  delay(1025);
}
