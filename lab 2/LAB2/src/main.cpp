// #define LED_PIN 48
// #define SDA_PIN GPIO_NUM_11
// #define SCL_PIN GPIO_NUM_12

// #include <WiFi.h>
// #include <Arduino_MQTT_Client.h>
// #include <ThingsBoard.h>
// #include "DHT.h"
// #include "Wire.h"
// #include <ArduinoOTA.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// constexpr char WIFI_SSID[] = "ACLAB";
// constexpr char WIFI_PASSWORD[] = "ACLAB2023";

// constexpr char TOKEN[] = "xt3lrdgj7oarkhkgqbp8";
// constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
// constexpr uint16_t THINGSBOARD_PORT = 1883U;

// constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
// constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// volatile bool attributesChanged = false;
// volatile int ledMode = 0;
// volatile bool ledState = false;
// volatile uint16_t blinkingInterval = 1000U;

// WiFiClient wifiClient;
// Arduino_MQTT_Client mqttClient(wifiClient);
// ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// DHT dht11(8, DHT11);  

// // Hàm kết nối WiFi
// void InitWiFi() {
//   Serial.println("Connecting to WiFi...");
//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   while (WiFi.status() != WL_CONNECTED) {
//     vTaskDelay(pdMS_TO_TICKS(500));
//     Serial.print(".");
//   }
//   Serial.println("Connected to WiFi");
// }

// // Task xử lý WiFi
// void WiFi_Task(void *pvParameters) {
//   while (1) {
//     if (WiFi.status() != WL_CONNECTED) {
//       Serial.println("WiFi lost. Reconnecting...");
//       InitWiFi();
//     }
//     vTaskDelay(pdMS_TO_TICKS(10000));  // Kiểm tra WiFi mỗi 5 giây
//   }
// }

// // Task điều khiển LED
// void LED_Task(void *pvParameters) {
  
//   while (1) {
//     if (ledMode == 1) {  
//       digitalWrite(LED_PIN, !digitalRead(LED_PIN));
//       Serial.print("LED state changed: ");
//       Serial.println(digitalRead(LED_PIN));
//       vTaskDelay(pdMS_TO_TICKS(blinkingInterval)); 
//     } else {
//       vTaskDelay(pdMS_TO_TICKS(100));  

//     }
//   }
// }

// // Task đọc cảm biến DHT11
// void Sensor_Task(void *pvParameters) {
//   dht11.begin();
//   while (1) {
//     float temperature = dht11.readTemperature();
//     float humidity = dht11.readHumidity();

//     if (!isnan(temperature)) {
//       Serial.print("Temp: ");
//       Serial.print(temperature);
//       Serial.print(" °C, Humidity: ");
//       Serial.println(humidity);
//       tb.sendTelemetryData("temperature", temperature);
//       tb.sendTelemetryData("humidity", humidity);
//     } else {
//       Serial.println("Failed to read from DHT11!");
//     }

//     vTaskDelay(pdMS_TO_TICKS(5000));  // Cập nhật mỗi 10 giây
//   }
// }

// // Task MQTT (ThingsBoard)
// void MQTT_Task(void *pvParameters) {
//   while (1) {
//     if (!tb.connected()) {
//       Serial.println("Connecting to ThingsBoard...");
//       if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
//         Serial.println("Failed to connect to ThingsBoard");
//         vTaskDelay(pdMS_TO_TICKS(5000));  
//         continue;
//       }
//       Serial.println("Connected to ThingsBoard!");
//     }

//     tb.loop();
//     vTaskDelay(pdMS_TO_TICKS(500));  
//   }
// }

// void setup() {
//   Serial.begin(SERIAL_DEBUG_BAUD);
//   InitWiFi();
//   pinMode(LED_PIN, OUTPUT);
//   // Tạo các Task RTOS
//   xTaskCreate(WiFi_Task, "WiFi Task", 4096, NULL, 1, NULL);
//   xTaskCreate(LED_Task, "LED Task", 2048, NULL, 1, NULL);
//   xTaskCreate(Sensor_Task, "Sensor Task", 4096, NULL, 1, NULL);
//   xTaskCreate(MQTT_Task, "MQTT Task", 4096, NULL, 1, NULL);
// }

// void loop() {
//   vTaskDelete(NULL);  
// }

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

volatile bool attributesChanged = false;
volatile int ledMode = 0;
volatile bool ledState = false;
volatile uint16_t blinkingInterval = 5000U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

DHT dht11(8, DHT11);  


volatile int ledmode = 0;  // 0: tắt, 1: bật

void onAttributesReceived(const JsonObjectConst& data) {
  if (data.containsKey("ledMode")) {
      ledmode = data["ledMode"];  // Cập nhật giá trị ledmode
      Serial.print("Updated LED mode: ");
      Serial.println(ledmode);
  }
}


void requestSharedAttributes() {
  tb.Shared_Attributes_Request(Attribute_Request_Callback(onAttributesReceived));

}

// Hàm kết nối WiFi
void InitWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}

// Task xử lý WiFi
void WiFi_Task(void *pvParameters) {
  Serial.println("WIFI");
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost. Reconnecting...");
      InitWiFi();
    }
    vTaskDelay(pdMS_TO_TICKS(10000));  // Kiểm tra WiFi mỗi 5 giây
  }
}

void LED_Task(void *pvParameters) {
  
  while (1) {
    if (ledmode == 1) {  
      digitalWrite(LED_PIN, HIGH);
      Serial.print("LED mode: ");
      Serial.println(ledmode);
      vTaskDelay(pdMS_TO_TICKS(blinkingInterval)); 
    } else {
      digitalWrite(LED_PIN, LOW);
      Serial.print("LED mode: ");
      Serial.println(ledmode);
      vTaskDelay(pdMS_TO_TICKS(5000));  
    }

  }
}


// Task đọc cảm biến DHT11
void Sensor_Task(void *pvParameters) {
  dht11.begin();
  while (1) {
    float temperature = dht11.readTemperature();
    float humidity = dht11.readHumidity();

    if (!isnan(temperature)) {
      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print(" °C, Humidity: ");
      Serial.println(humidity);
      tb.sendTelemetryData("temperature", temperature);
      tb.sendTelemetryData("humidity", humidity);
    } else {
      Serial.println("Failed to read from DHT11!");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));  // Cập nhật mỗi 10 giây
  }
}

// Task MQTT (ThingsBoard)
void MQTT_Task(void *pvParameters) {
  Serial.println("MQTT");
  while (1) {
      if (!tb.connected()) {
          Serial.println("Connecting to ThingsBoard...");
          if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
              Serial.println("Failed to connect to ThingsBoard");
              vTaskDelay(pdMS_TO_TICKS(5000));
              continue;
          }
          Serial.println("Connected to ThingsBoard!");
          // tb.subscribeSharedAttributes(onAttributesReceived); 
          tb.Shared_Attributes_Subscribe(Shared_Attribute_Callback(onAttributesReceived));
          requestSharedAttributes();  // Yêu cầu giá trị ledmode khi kết nối thành công
      }

      tb.loop();
      vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  InitWiFi();
  pinMode(LED_PIN, OUTPUT);

  // Tạo các Task RTOS
  xTaskCreate(WiFi_Task, "WiFi Task", 8192, NULL, 1, NULL);
  xTaskCreate(MQTT_Task, "MQTT Task", 8192, NULL, 1, NULL);
  xTaskCreate(LED_Task, "LED Task", 4096, NULL, 3, NULL);
  xTaskCreate(Sensor_Task, "Sensor Task", 4096, NULL, 2, NULL);
  
}

void loop() {
  vTaskDelete(NULL);  
}

