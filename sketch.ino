#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Định nghĩa chân kết nối phần cứng
#define DHTPIN 22          // Chân dữ liệu DHT22
#define DHTTYPE DHT22      // Loại cảm biến
#define PUMP_PIN 17        // Relay Máy bơm (GPIO 17)
#define FAN_PIN 18         // Relay Quạt (GPIO 18)
#define MIST_PIN 19        // LED Phun sương PWM (GPIO 19)

// Cấu hình PWM cho máy phun sương
#define PWM_FREQ 5000      // Tần số 5 kHz
#define PWM_RESOLUTION 8   // Độ phân giải 8-bit (0 - 255)

// Thông tin WiFi & MQTT Broker (Sử dụng broker công cộng EMQX)
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// Các biến trạng thái thiết bị
bool pumpState = false;
bool fanState = false;

// Queue giao tiếp giữa các Task FreeRTOS
QueueHandle_t dataQueue;
QueueHandle_t alertQueue;

struct SensorData {
  float temperature;
  float humidity;
};

// Callback nhận lệnh từ MQTT (Web điều khiển thủ công)
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Nhan lenh MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "esp32/garden/pump") {
    pumpState = (message == "ON");
    digitalWrite(PUMP_PIN, pumpState ? HIGH : LOW);
  }
  else if (String(topic) == "esp32/garden/fan") {
    fanState = (message == "ON");
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT Broker...");
    String clientId = "ESP32Client-LeMinhQuan-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Da ket noi!");
      client.subscribe("esp32/garden/pump");
      client.subscribe("esp32/garden/fan");
    } else {
      Serial.print("That bai, rc=");
      Serial.print(client.state());
      Serial.println(" Thu lai sau 5 giay");
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

// ================= THREAD CORE 0: Task truyen thong WiFi & MQTT =================
void TaskTransmit(void *pvParameters) {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("\nWiFi da ket noi!");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  SensorData receivedData;

  for (;;) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    // Nhận dữ liệu từ Queue và publish lên MQTT Broker
    if (xQueueReceive(dataQueue, &receivedData, 100 / portTICK_PERIOD_MS) == pdTRUE) {
      char payload[100];
      sprintf(payload, "{\"temp\":%.1f,\"hum\":%.1f,\"pump\":%d,\"fan\":%d}", 
              receivedData.temperature, receivedData.humidity, pumpState, fanState);
      
      client.publish("esp32/garden/telemetry", payload);
      Serial.println("Da gui du lieu len MQTT Broker!");
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS); // Gửi mỗi 2 giây
  }
}

// ================= THREAD CORE 1: Task đọc cảm biến =================
void TaskRead(void *pvParameters) {
  dht.begin();
  SensorData data;

  for (;;) {
    data.humidity = dht.readHumidity();
    data.temperature = dht.readTemperature();

    if (isnan(data.humidity) || isnan(data.temperature)) {
      Serial.println("Loi doc cam bien DHT22!");
    } else {
      Serial.printf("Nhiet do: %.1f C | Do am: %.1f %%\n", data.temperature, data.humidity);
      
      // Gửi dữ liệu sang Queue truyền thông
      xQueueSend(dataQueue, &data, portMAX_DELAY);
      // Gửi dữ liệu độ ẩm sang Queue cảnh báo phun sương
      xQueueSend(alertQueue, &data.humidity, portMAX_DELAY);
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ================= THREAD CORE 1: Task điều khiển Máy phun sương (PWM) =================
void TaskAlert(void *pvParameters) {
  // Cấu hình PWM cho chân LED phun sương
  ledcAttach(MIST_PIN, PWM_FREQ, PWM_RESOLUTION);

  float hum;
  for (;;) {
    if (xQueueReceive(alertQueue, &hum, portMAX_DELAY) == pdTRUE) {
      int pwmVal = 0;
      
      // Thuật toán phun sương theo độ ẩm
      if (hum < 30.0) {
        pwmVal = 255;  // Dưới 30%: 100% công suất
        Serial.println("[Canh bao] Do am duoi 30% -> Phun suong 100%");
      } else if (hum >= 30.0 && hum < 35.0) {
        pwmVal = 128;  // 30-35%: 50% công suất
        Serial.println("[Canh bao] Do am 30-35% -> Phun suong 50%");
      } else if (hum >= 35.0 && hum <= 40.0) {
        pwmVal = 64;   // 35-40%: 25% công suất
        Serial.println("[Canh bao] Do am 35-40% -> Phun suong 25%");
      } else {
        pwmVal = 0;    // Trên 40%: Tắt phun sương
        Serial.println("Do am on dinh -> Tat phun suong");
      }

      ledcWrite(MIST_PIN, pwmVal);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Khởi tạo các Queue
  dataQueue = xQueueCreate(5, sizeof(SensorData));
  alertQueue = xQueueCreate(5, sizeof(float));

  // Tạo Task chạy trên Core 0 (Truyền thông WiFi/MQTT)
  xTaskCreatePinnedToCore(
    TaskTransmit,   // Tên hàm Task
    "TaskTransmit", // Tên Task
    4096,           // Kích thước Stack
    NULL,           // Tham số
    1,              // Độ ưu tiên
    NULL,           
    0               // Gán vào Core 0
  );

  // Tạo Task chạy trên Core 1 (Đọc cảm biến)
  xTaskCreatePinnedToCore(
    TaskRead, 
    "TaskRead", 
    2048, 
    NULL, 
    2, 
    NULL, 
    1               // Gán vào Core 1
  );

  // Tạo Task chạy trên Core 1 (Điều khiển phun sương PWM)
  xTaskCreatePinnedToCore(
    TaskAlert, 
    "TaskAlert", 
    2048, 
    NULL, 
    2, 
    NULL, 
    1               // Gán vào Core 1
  );
}

void loop() {
  // FreeRTOS đã quản lý các Task, hàm loop để trống
}