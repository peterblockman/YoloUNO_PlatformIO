# Video 5–7: PlatformIO, tác vụ LED và DHT20

Dự án độc lập cho bo YoloUNO ESP32-S3. LED đơn dùng GPIO48. DHT20 nối vào cổng I²C đỏ, SDA11 và SCL12.

## Môi trường

- VS Code với phần mở rộng `platformio.platformio-ide`.
- PlatformIO Core, Arduino framework và `espressif32@7.1.3`.
- Cấu hình bo trong `boards/yolo_uno.json`.
- Thư viện DHT20 0.2.2 cục bộ trong `lib/DHT20`, giữ kèm giấy phép thư viện.
- Cửa sổ nối tiếp: 115200 baud.

## Chạy từng bài

```sh
pio run -e video6
pio run -e video6 -t upload
pio device monitor -b 115200
```

`video6` chỉ tạo tác vụ LED. Mỗi vòng lặp đảo trạng thái rồi gọi `vTaskDelay(pdMS_TO_TICKS(2000))`, tương ứng khoảng 2 giây sáng và 2 giây tắt.

```sh
pio run -e video7
pio run -e video7 -t upload
pio device monitor -b 115200
```

`video7` tạo hai tác vụ cùng mức ưu tiên 2: LED và DHT20. Cảm biến được khởi tạo bên trong tác vụ DHT20, chỉ tác vụ này truy cập I²C. Sau mỗi lần đọc, tác vụ chờ 5 giây. Khoảng giữa hai mẫu còn gồm thời gian đọc cảm biến và lập lịch.

Khóa mutex bảo vệ việc in nhật ký từ hai tác vụ. `loop()` chỉ nhường CPU. Khi cảm biến lỗi, chương trình in mã lỗi và thử khởi tạo lại, không thay bằng dữ liệu mô phỏng.

Trên macOS có thể chọn cổng với `--upload-port /dev/cu.usbmodem1101`. Tên cổng có thể thay đổi khi cắm lại USB. Đóng cửa sổ nối tiếp trước khi nạp chương trình khác.
