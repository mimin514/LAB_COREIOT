#define LED_PIN 13
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

constexpr char WIFI_SSID[] = "ACLAB";
constexpr char WIFI_PASSWORD[] = "ACLAB2023";
constexpr char TOKEN[] = "i5Zu2KNmPlkALJxTwlyJ";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

const String firmware_url = "http://app.coreiot.io:8080/api/v1/i5Zu2KNmPlkALJxTwlyJ/firmware"; // Đường dẫn OTA firmware

volatile int ledMode = 0;
volatile uint16_t blinkingInterval = 5000U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);
DHT dht11(27, DHT11);

void onAttributesReceived(const JsonObjectConst &data) {
  if (data.containsKey("ledMode")) {
    ledMode = data["ledMode"];
    Serial.printf("Updated LED mode: %d\n", ledMode);
  }
}

void requestSharedAttributes() {
  tb.Shared_Attributes_Request(Attribute_Request_Callback(onAttributesReceived));
}

void InitWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());
}

void WiFi_Task(void *pvParameters) {
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost. Reconnecting...");
      InitWiFi();
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void LED_Task(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  while (1) {
    digitalWrite(LED_PIN, ledMode ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(blinkingInterval));
  }
}

void Sensor_Task(void *pvParameters) {
  dht11.begin();
  while (1) {
    float temperature = dht11.readTemperature();
    float humidity = dht11.readHumidity();
    if (!isnan(temperature)) {
      Serial.printf("Temp: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      tb.sendTelemetryData("temperature", temperature);
      tb.sendTelemetryData("humidity", humidity);
    } else {
      Serial.println("Failed to read from DHT11!");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

RPC_Response setLedModeCallback(const RPC_Data &data) {
  Serial.println("Received RPC call: setLedMode");
  if (data.containsKey("params") && data["params"].containsKey("ledMode")) {
    ledMode = data["params"]["ledMode"];
    Serial.printf("LED mode updated: %d\n", ledMode);
    digitalWrite(LED_PIN, ledMode ? HIGH : LOW);
    return RPC_Response("LED mode updated", true);
  }
  return RPC_Response("Error: No ledMode parameter", false);
}

void MQTT_Task(void *pvParameters) {
  while (1) {
    if (!tb.connected()) {
      Serial.println("Connecting to ThingsBoard...");
      unsigned long startAttemptTime = millis();
      const unsigned long connectionTimeout = 5000;
      while (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        if (millis() - startAttemptTime > connectionTimeout) {
          Serial.println("MQTT connect timeout, will retry...");
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
      if (tb.connected()) {
        Serial.println("Connected to ThingsBoard!");
        tb.RPC_Subscribe(RPC_Callback("setLedMode", setLedModeCallback));
        tb.Shared_Attributes_Subscribe(Shared_Attribute_Callback(onAttributesReceived));
        requestSharedAttributes();
      }
    }

    tb.loop();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ✅ OTA từ xa: tải firmware từ ThingsBoard
void checkFirmwareUpdate() {
  Serial.println("Checking for firmware update...");
  HTTPClient http;
  http.begin(firmware_url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    int len = http.getSize();
    WiFiClient *stream = http.getStreamPtr();
    if (Update.begin(len)) {
      size_t written = Update.writeStream(*stream);
      if (written == len) {
        Serial.println("Firmware written successfully. Rebooting...");
        if (Update.end(true)) {
          ESP.restart();
        } else {
          Serial.printf("Update failed: %s\n", Update.errorString());
        }
      } else {
        Serial.println("Firmware size mismatch");
        Update.end(false);
      }
    } else {
      Serial.println("Failed to begin update");
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
  }
  http.end();
}

void FirmwareUpdate_Task(void *pvParameters) {
  while (1) {
    checkFirmwareUpdate();  // kiểm tra cập nhật mới
    vTaskDelay(pdMS_TO_TICKS(60000)); // mỗi 1 phút kiểm tra lại
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  InitWiFi();
  xTaskCreate(WiFi_Task, "WiFi Task", 4096, NULL, 1, NULL);
  xTaskCreate(MQTT_Task, "MQTT Task", 8192, NULL, 1, NULL);
  xTaskCreate(LED_Task, "LED Task", 4096, NULL, 1, NULL);
  xTaskCreate(Sensor_Task, "Sensor Task", 4096, NULL, 2, NULL);
  xTaskCreate(FirmwareUpdate_Task, "FirmwareUpdate Task", 8192, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
