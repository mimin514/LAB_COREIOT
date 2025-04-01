#define LED_PIN 48
#define SDA_PIN GPIO_NUM_11
#define SCL_PIN GPIO_NUM_12

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT.h"
#include "Wire.h"
#include <ArduinoOTA.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

constexpr char WIFI_SSID[] = "";
constexpr char WIFI_PASSWORD[] = "";

constexpr char TOKEN[] = "";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

volatile int ledMode = 0;
volatile uint16_t blinkingInterval = 5000U;
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);
DHT dht11(8, DHT11);

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
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.println("Connected to WiFi");
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
        Serial.printf("LED mode: %d\n", ledMode);
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
  Serial.print("Raw data received: ");
  Serial.println(data.as<String>());   
  if (data.containsKey("params") && data["params"].containsKey("ledMode")) {
     ledMode = data["params"]["ledMode"];
    Serial.printf("LED mode updated: %d\n", ledMode);
    
    digitalWrite(LED_PIN, ledMode ? HIGH : LOW);
    return RPC_Response("LED mode updated", true);
}

  Serial.println(" Error: No ledMode parameter found in RPC call.");
  return RPC_Response("Error: No ledMode parameter", false);
}

void MQTT_Task(void *pvParameters) {
    while (1) {
        if (!tb.connected()) {
            Serial.println("Connecting to ThingsBoard...");
            if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
                Serial.println("Failed to connect to ThingsBoard");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            Serial.println("Connected to ThingsBoard!");
            tb.RPC_Subscribe(RPC_Callback("setLedMode", setLedModeCallback));

            // tb.RPC_Subscribe(RPC_Callback("processData", processDataCallback));
            tb.Shared_Attributes_Subscribe(Shared_Attribute_Callback(onAttributesReceived));
            requestSharedAttributes();
        }
        tb.loop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void setup() {
    Serial.begin(SERIAL_DEBUG_BAUD);
    InitWiFi();
    xTaskCreate(WiFi_Task, "WiFi Task", 4096, NULL, 1, NULL);
    xTaskCreate(MQTT_Task, "MQTT Task", 8192, NULL, 1, NULL);
    xTaskCreate(LED_Task, "LED Task", 4096, NULL, 2, NULL);
    xTaskCreate(Sensor_Task, "Sensor Task", 4096, NULL, 2, NULL);
}

void loop() {
    vTaskDelete(NULL);
}
