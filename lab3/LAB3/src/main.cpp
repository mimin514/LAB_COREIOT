#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

const char* ssid = "NhuHa 2.4G";
const char* password = "Nhuha1972@";
const char* mqtt_server = "app.coreiot.io"; // CoreIoT server
const int mqtt_port = 1883;
const char* token = "jyc9u1tesgpc55n4zelk"; // Access token from CoreIoT

const char* current_version = "1"; // Current firmware version

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Function to handle OTA update
void doOTAUpdate(String firmwareUrl) {
  HTTPClient http;
  http.begin(firmwareUrl); // Firmware URL
  int httpCode = http.GET();

  if (httpCode == 200) {
    int contentLength = http.getSize();
    if (Update.begin(contentLength)) {
      WiFiClient* stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);
      if (Update.end() && Update.isFinished()) {
        Serial.println("✅ Cập nhật thành công. Restarting...");
        delay(2000);  // Wait for 2 seconds before restarting
        ESP.restart(); // Restart ESP32 to apply the update
      } else {
        Serial.println("❌ OTA lỗi.");
      }
    }
  } else {
    Serial.println("❌ Không tải được file OTA.");
  }
  http.end(); // Close HTTP connection
}

// Callback function for MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("📩 Nhận phản hồi: " + message);

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("❌ Lỗi phân tích JSON: " + String(error.c_str()));
    return;  // Dừng lại nếu lỗi phân tích JSON
  }

  String newVersion = doc["fw_version"];
  String newUrl = doc["fw_url"];

  // Kiểm tra nếu fw_url hoặc fw_version không hợp lệ
  if (newUrl == "" || newVersion == "") {
    Serial.println("❌ URL hoặc phiên bản không hợp lệ: " + newUrl + ", " + newVersion);
    return; // Dừng lại nếu URL hoặc phiên bản không hợp lệ
  }

  Serial.println("🚀 Có bản mới: " + newVersion + " → OTA từ " + newUrl);
  if (newVersion != current_version) {
    doOTAUpdate(newUrl);  // Gọi OTA nếu có bản mới
  } else {
    Serial.println("✅ Phiên bản đã là mới nhất.");
  }
}

// Function to request attributes (fw_version, fw_url) from CoreIoT
void requestAttributes() {
  String topic = "v1/devices/me/attributes/request/1";  // Request attributes topic
  String payload = "{\"sharedKeys\":\"fw_version,fw_url\"}"; // JSON payload for attributes
  client.publish(topic.c_str(), payload.c_str()); // Publish request to CoreIoT
}

// Function to reconnect to MQTT server
void reconnect() {
  while (!client.connected()) {
    Serial.print("⏳ Kết nối MQTT...");
    if (client.connect("esp32_client", token, nullptr)) {  // Connect using token
      Serial.println("✅ Kết nối MQTT thành công");
      client.subscribe("v1/devices/me/attributes/response/1"); // Subscribe to response topic
      requestAttributes(); // Request for attributes
    } else {
      Serial.print("❌ Thất bại. Lý do: ");
      Serial.println(client.state()); // Print failure reason
      delay(2000); // Wait 2 seconds before retrying
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password); // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n🔌 WiFi connected");

  client.setServer(mqtt_server, mqtt_port); // Set MQTT server
  client.setCallback(callback); // Set callback for MQTT messages
}

void loop() {
  if (!client.connected()) {
    reconnect(); // Reconnect to MQTT if disconnected
  }
  client.loop(); // Process MQTT messages
}
