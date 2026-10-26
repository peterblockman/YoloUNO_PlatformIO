# Video 8: DHT20 và điều khiển LED qua CoreIOT với FreeRTOS

Bo YoloUNO ESP32-S3, DHT20 ở SDA11/SCL12, LED đơn GPIO48. Bài dùng lại thiết bị CoreIOT của bo đã tạo ở tuần 5, không dùng thiết bị Python mô phỏng.

## Kiến trúc

| Tác vụ | Thời gian | Chức năng |
| --- | --- | --- |
| DHT20 | Chờ 5 giây sau mỗi lần đọc | Chỉ tác vụ này truy cập cảm biến, cập nhật mẫu dưới mutex |
| WiFi | Kiểm tra mỗi giây, thử nối lại cách ít nhất 10 giây | Quản lý kết nối Wi-Fi |
| Telemetry | Chu kỳ 10 giây với vTaskDelayUntil | Chuyển mẫu hợp lệ, không quá 15 giây vào hàng đợi một phần tử |
| MQTT | Chờ 10 ms giữa hai vòng | Duy trì kết nối, đăng ký RPC, gửi số đo và thuộc tính. Thử nối lại cách ít nhất 5 giây |
| SerialConfig | Chờ 10 ms | Nhận cấu hình qua USB, không in thông tin truy cập |

Chỉ tác vụ MQTT gọi PubSubClient và truy cập trạng thái LED. Hàng đợi tách lịch gửi số đo khỏi tác vụ MQTT, tránh truy cập máy khách MQTT đồng thời. `loop()` chỉ nhường bộ xử lý. Thời gian thực tế của một vòng MQTT còn phụ thuộc các lời gọi mạng.

Video minh họa bộ phát triển ThingsBoard SDK. Bản thực hành này dùng PubSubClient 2.8 và ArduinoJson 6.21.5 cục bộ để thực hiện trực tiếp cùng giao thức MQTT của CoreIOT. DHT20 dùng bản 0.2.2. Đây là thay đổi ở lớp thư viện, giữ các chức năng kết nối lại, gửi số đo, thuộc tính và RPC của bài. Giấy phép gốc đi kèm các thư viện.

## Biên dịch và nạp

```sh
pio run -e video8
pio run -e video8 -t upload
pio device monitor -b 115200
```

Tên cổng USB thay đổi tùy máy. Đóng cửa sổ nối tiếp trước khi nạp chương trình khác.

## Cấu hình khi chạy

Gửi từng dòng JSON qua USB. Tự thay giá trị trong dấu ngoặc nhọn bên dưới, không đưa cấu hình thật vào kho mã nguồn.

```json
{"cmd":"wifi","ssid":"<ten-wifi-2.4GHz>","pass":"<mat-khau>"}
{"cmd":"mqtt","token":"<ma-truy-cap-thiet-bi-CoreIOT>"}
```

Thông tin truy cập chỉ giữ trong RAM, mất khi khởi động lại. `WiFi.persistent(false)` tắt lưu cấu hình Wi-Fi xuống bộ nhớ flash. Cấu hình mới có thể gửi lại mà không sửa mã hoặc biên dịch lại.

Các lệnh thử kết nối lại có chủ đích:

```json
{"cmd":"reconnect_mqtt"}
{"cmd":"reconnect_wifi"}
{"cmd":"status"}
```

## Cấu hình CoreIOT

- Máy chủ `app.coreiot.io`, cổng `1883`.
- Số đo gửi tới `v1/devices/me/telemetry`: `temperature`, `humidity`, `sampleSequence`, `source=DHT20`.
- Thuộc tính gửi tới `v1/devices/me/attributes`: `ledState`, `macAddress`, `localIp`, `ssid`, `rssi`, `channel`, `firmware`.
- Đăng ký `v1/devices/me/rpc/request/+`, trả kết quả về `v1/devices/me/rpc/response/<request_id>`.
- Công tắc dùng `getLedSwitchValue` để lấy trạng thái và `setLedSwitchValue` với tham số boolean để bật/tắt. Tham số sai kiểu hoặc phương thức không hỗ trợ nhận phản hồi lỗi.
- Sau mỗi lần kết nối MQTT thành công, đăng ký RPC lại và gửi thuộc tính hiện tại. Không thay dữ liệu cảm biến lỗi bằng số liệu mô phỏng. Số đo quá 15 giây bị loại.

`local_write_ok=1` chỉ xác nhận thao tác ghi ở phía máy khách. Cần kiểm tra Latest telemetry và Attributes trên CoreIOT để xác nhận dữ liệu đã tới máy chủ. Báo cáo kèm ảnh kết quả và nhật ký chạy thật.
