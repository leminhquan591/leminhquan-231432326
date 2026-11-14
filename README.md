# leminhquan-231432319# Lab #3 - Giám sát môi trường vườn cây công nghiệp (ESP32 + FreeRTOS + MQTT)

## Thông tin sinh viên

- **Họ tên:** Lê Minh Quân
- **Mã sinh viên:** 231432326
- **Lớp:** K64 Điện tử Viễn thông
- **Trường:** Đại học Giao thông Vận tải

---

## Mô tả đề tài

Hệ thống sử dụng ESP32 đọc nhiệt độ và độ ẩm từ cảm biến DHT22, gửi dữ liệu lên MQTT Broker công cộng (`broker.emqx.io`) và hiển thị trực tiếp trên Web Dashboard.

Hệ thống hỗ trợ:

- Giám sát nhiệt độ và độ ẩm theo thời gian thực.
- Điều khiển máy bơm và quạt từ xa thông qua giao diện Web.
- Tự động điều khiển máy phun sương dựa trên giá trị độ ẩm môi trường bằng PWM.
- Trao đổi dữ liệu theo giao thức MQTT.

---

## Kiến trúc FreeRTOS

| Task | Core | Chức năng |
|------|------|------------|
| `TaskTransmit` | 0 | Xử lý WiFi/MQTT, đọc `dataQueue` để gửi dữ liệu Telemetry và nhận lệnh điều khiển từ Web |
| `TaskRead` | 1 | Đọc cảm biến DHT22, hiển thị Serial Monitor, gửi dữ liệu vào `dataQueue` và `alertQueue` |
| `TaskAlert` | 1 | Nhận dữ liệu từ `alertQueue`, điều khiển máy phun sương theo độ ẩm |

---

## Sơ đồ chân kết nối

| Thiết bị | GPIO |
|-----------|------|
| DHT22 | GPIO 22 |
| Relay máy bơm | GPIO 17 |
| Relay quạt gió | GPIO 18 |
| Máy phun sương (PWM) | GPIO 19 |

---

## Thuật toán điều khiển phun sương

| Độ ẩm | Công suất phun sương |
|--------|--------------------|
| < 30% | 100% (PWM = 255) |
| 30% - < 35% | 50% (PWM = 128) |
| 35% - 40% | 25% (PWM = 64) |
| > 40% | Tắt (PWM = 0) |

---

## Cấu trúc thư mục

```text
LAB3_RTOS_ESP32/
│
├── sketch.ino          # Chương trình ESP32 sử dụng FreeRTOS
├── diagram.json        # Mô phỏng phần cứng trên Wokwi
├── libraries.txt       # Danh sách thư viện cần cài đặt
├── index.html          # Web Dashboard điều khiển và giám sát
└── Bao_cao_Lab3_Le_Minh_Quan_231432326_baocao.docx
```

---

## Công nghệ sử dụng

- ESP32
- FreeRTOS
- MQTT
- DHT22
- HTML/CSS/JavaScript
- Wokwi Simulator

---

## Link mô phỏng Wokwi

🔗 https://wokwi.com/projects/480543063497655297

---

## Kết quả đạt được

- ESP32 đọc dữ liệu cảm biến DHT22 thành công.
- Truyền dữ liệu lên MQTT Broker.
- Hiển thị dữ liệu thời gian thực trên Web Dashboard.
- Điều khiển máy bơm và quạt từ xa.
- Điều khiển công suất máy phun sương tự động dựa trên độ ẩm.

---

## Tác giả

**Lê Minh Quân**  
MSSV: 231432326  
K64 Điện tử Viễn thông - Đại học Giao thông Vận tải
