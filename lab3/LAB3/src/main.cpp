#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Biến giả lập URL firmware OTA nhận từ ThingsBoard
String firmwareUrl = "https://github.com/mimin514/LAB_COREIOT/tree/main/lab3/LAB3/firmware_v2.bin"; // sẽ giả lập nhận từ MQTT

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  dht.begin();

  // Mô phỏng nhận OTA từ ThingsBoard
  mockReceiveFirmwareUrl();
}

void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  Serial.printf("Nhiệt độ: %.1f°C | Độ ẩm: %.1f%%\n", temp, hum);
  delay(5000);
}

// Hàm mô phỏng nhận firmware URL từ ThingsBoard
void mockReceiveFirmwareUrl() {
  Serial.println("\n[ThingsBoard] Đã nhận yêu cầu cập nhật OTA.");
  Serial.println("[ThingsBoard] Link firmware mới: " + firmwareUrl);

  // Mô phỏng tải file .bin
  Serial.println("[ESP32] Đang tải firmware từ: " + firmwareUrl);
  delay(2000); // mô phỏng thời gian tải

  Serial.println("[ESP32] OTA giả lập thành công (Wokwi không hỗ trợ OTA thật).");
  Serial.println("[ESP32] Thiết bị sẽ khởi động lại...\n");
  delay(2000);
  // ESP.restart(); // Không cần restart thật trong Wokwi
}
