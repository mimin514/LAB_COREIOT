
#define CONFIG_THINGSBOARD_ENABLE_DEBUG false
// #define THINGSBOARD_ENABLE_STL
#include <Arduino.h>
#include <ThingsBoard.h>
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <OTA_Firmware_Update.h>
#include <Shared_Attribute_Update.h>
#include <Attribute_Request.h>
#include <Espressif_Updater.h>
#include <DHT.h>
#include <Wire.h>
#include "RPC_Callback.h"
// #include <PubSubClient.h> 

#define CURRENT_FIRMWARE_TITLE "OTA"
#define LED_PIN 13
#define CURRENT_FIRMWARE_VERSION "3.2"
#define DHT_PIN 19

constexpr int16_t TELEMETRY_SEND_INTERVAL = 5000U;
constexpr uint8_t FIRMWARE_FAILURE_RETRIES = 12U;
constexpr uint16_t FIRMWARE_PACKET_SIZE = 4096U;

constexpr char WIFI_SSID[] = "";
constexpr char WIFI_PASSWORD[] = "";
constexpr char TOKEN[] = "";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr char TEMPERATURE_KEY[] = "temperature";
constexpr char HUMIDITY_KEY[] = "humidity";
constexpr uint16_t THINGSBOARD_PORT = 1883U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 1024U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;
constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 10000U * 1000U;
constexpr int16_t telemetrySendInterval = 3000U;
constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;
constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[] = "ledMode";
constexpr char LED_STATE_ATTR[] = "ledState";
constexpr uint8_t MAX_ATTRIBUTES = 2U;
constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};

WiFiClient wifi_client;
Arduino_MQTT_Client mqttClient(wifi_client);
DHT dht(DHT_PIN, DHT11); //DHT11

OTA_Firmware_Update<> ota;
Shared_Attribute_Update<1U, MAX_ATTRIBUTES> shared_update;
Attribute_Request<2U, MAX_ATTRIBUTES> attr_request;
const std::array<IAPI_Implementation*, 3U> apis = { &shared_update, &attr_request, &ota };
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE, Default_Max_Stack_Size, apis);
Espressif_Updater<> updater;

volatile bool attributesChanged = false;
volatile bool ledState = false;
volatile bool fanState = false;
bool shared_update_subscribed = false;
bool currentFWSent = false;
bool updateRequestSent = false;
bool requestedShared = false;

String latestFirmwareVersion = CURRENT_FIRMWARE_VERSION; // Dùng để kiểm tra phiên bản mới

void processSharedAttributes(const JsonObjectConst &data) {
  Serial.println("Process shared attributes");
  if (data.containsKey(BLINKING_INTERVAL_ATTR)) {
    const uint16_t new_interval = data[BLINKING_INTERVAL_ATTR].as<uint16_t>();
    if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX) {
      blinkingInterval = new_interval;
      Serial.print("Blinking interval is set to: ");
      Serial.println(new_interval);
    }
  }

  if (data.containsKey(LED_STATE_ATTR)) {
    ledState = data[LED_STATE_ATTR].as<bool>();
    digitalWrite(LED_PIN, bool(ledState));
    Serial.print("LED state is set to: ");
    Serial.println(ledState);
  }

  attributesChanged = true;
}

void requestTimedOut() {
  Serial.printf("Attribute request timed out after %llu microseconds.\n", REQUEST_TIMEOUT_MICROSECONDS);
}

void update_starting_callback() {}

void finished_callback(const bool & success) {
  if (success) {
    Serial.println("Done, Reboot now");
    esp_restart();
  } else {
    Serial.println("Downloading firmware failed");
    updateRequestSent = false;
  }
}

void progress_callback(const size_t & current, const size_t & total) {
  Serial.printf("Progress %.2f%%\n", static_cast<float>(current * 100U) / total);
}

void processSharedAttributeUpdate(const JsonObjectConst &data) {
  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}

void processSharedAttributeRequest(const JsonObjectConst &data) {
  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}

void taskWifiConnection(void* pvParameters){
    Serial.println("Connecting to wifi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while(1){
      // Serial.printf("WiFi status: %d\n", WiFi.status());
      if(WiFi.status() != WL_CONNECTED){
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      vTaskDelay(10000);
    }
  }

void taskThingsBoard(void* pvParameters){
    while (1){
      if (!tb.connected()){
        Serial.println("Connecting to ThingsBoard...");
        if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)){
          Serial.println("Failed to connect");
          vTaskDelay(5000);
          continue;
        }
        Serial.println("Connected to ThingsBoard");
        if (!requestedShared) {
          const Attribute_Request_Callback<MAX_ATTRIBUTES> sharedCallback(&processSharedAttributeRequest, REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut, SHARED_ATTRIBUTES_LIST);
          requestedShared = attr_request.Shared_Attributes_Request(sharedCallback);
        }
  
        if (!shared_update_subscribed) {
          const Shared_Attribute_Callback<MAX_ATTRIBUTES> callback(&processSharedAttributeUpdate, SHARED_ATTRIBUTES_LIST);
          shared_update_subscribed = shared_update.Shared_Attributes_Subscribe(callback);
        }
      }

      // Check if firmware version is different and update if necessary
      if (!currentFWSent && latestFirmwareVersion != CURRENT_FIRMWARE_VERSION) {
        currentFWSent = ota.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION);
      }
  
      if (!updateRequestSent) {
        const OTA_Update_Callback callback(
          CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION,
          &updater,
          &finished_callback,
          &progress_callback,
          &update_starting_callback,
          FIRMWARE_FAILURE_RETRIES,
          FIRMWARE_PACKET_SIZE
        );
  
        bool started = ota.Start_Firmware_Update(callback);
        bool subscribed = ota.Subscribe_Firmware_Update(callback);
      
        if (started && subscribed) {
          Serial.println("Firmware Update Started & Subscribed.");
          updateRequestSent = true;
        } else {
          Serial.println("Firmware Update FAILED to start or subscribe.");
        }
      }    
      tb.loop();
      vTaskDelay(3000);
    }
}

void taskDHT11(void * pvParameters){
    while(1){
      float temperature = dht.readTemperature();
      float humidity = dht.readHumidity();
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" °C, Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
        tb.sendTelemetryData("temperature", temperature);
        tb.sendTelemetryData("humidity", humidity);
      vTaskDelay(3000);
    }
}

void setup() {
  pinMode(LED_PIN,OUTPUT);
  dht.begin();
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);
  xTaskCreatePinnedToCore(taskWifiConnection, "WiFi Connection", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskThingsBoard, "Thingsboard Connection", 8192, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(taskDHT11, "Temperature and Humidity", 4096, NULL, 1, NULL, 1);
}

void loop() {
}
