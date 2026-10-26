# Video 9–11: các mô-đun tác vụ LED, NeoPixel và DHT20

YoloUNO ESP32-S3, LED đơn GPIO48, NeoPixel GPIO45, DHT20 SDA11/SCL12. Không cần LCD. Các bài được bật dần qua ba cấu hình:

```sh
pio run -e video9 -e video10 -e video11
pio run -e video11 -t upload
pio device monitor -b 115200
```

`video9` chạy LED và tác vụ nhận lệnh USB. `video10` thêm NeoPixel. `video11` thêm DHT20. Chọn `-e video9` hoặc `-e video10` khi cần nạp riêng hai bài đầu.

## Các tệp

- `global.h` khai báo `SharedState` và các biến `extern`, `global.cpp` định nghĩa chúng một lần. Khóa tương hỗ bảo vệ dữ liệu chung, khóa riêng bảo vệ nhật ký.
- `led_blinky.h/.cpp` chứa tác vụ LED đơn.
- `neo_blinky.h/.cpp` chứa tác vụ NeoPixel, chỉ tác vụ này sử dụng đối tượng Adafruit_NeoPixel.
- `temp_humi_monitor.h/.cpp` chứa tác vụ DHT20, chỉ tác vụ này truy cập cảm biến/I²C.
- `main.cpp` khởi tạo khóa, tạo tác vụ theo cấu hình và nhận lệnh USB. `loop()` chỉ nhường thời gian xử lý.

## Chế độ thủ công

Gửi một dòng `mode L N`, nhấn Enter. Lệnh tắt chế độ tự động. Tác vụ áp dụng chế độ mới ở đầu chu kỳ tiếp theo.

| L | LED đơn |
| --- | --- |
| 0 | Sáng 1.000 ms, tắt 1.000 ms |
| 1 | Sáng 100 ms, tắt 900 ms |
| 2 | Sáng 100 ms, tắt 100 ms, sáng 100 ms, tắt 1.700 ms |

| N | NeoPixel, sáng 500 ms rồi tắt 500 ms |
| --- | --- |
| 0 | Luân phiên đỏ, xanh lá, xanh dương, mỗi kênh có giá trị 64 |
| 1 | Đỏ, RGB=(64,0,0) |
| 2 | Xanh lá, RGB=(0,64,0) |
| 3 | Xanh dương, RGB=(0,0,64) |
| 4 | Xanh lá mức thấp, RGB=(0,8,0) |

Ví dụ `mode 2 0` tạo hiệu ứng LED lóe hai lần và NeoPixel đổi ba màu. Hai tác vụ có nhịp riêng.

## DHT20 và dữ liệu chung

Tác vụ đọc sau mỗi khoảng chờ 5 giây, kiểm tra trạng thái và giá trị hữu hạn, rồi ghi nhiệt độ, độ ẩm và số thứ tự mẫu vào `sharedState`. Không tạo dữ liệu mô phỏng khi cảm biến lỗi.

Gửi `auto 30` để dùng ngưỡng nhiệt độ 30 °C minh họa:

- Nhiệt độ từ ngưỡng trở lên: LED lóe hai lần, NeoPixel đỏ.
- Nhiệt độ dưới ngưỡng: LED chớp đều, NeoPixel xanh lá.

Đây là quy tắc hiển thị phục vụ bài thực hành, không phải giới hạn an toàn môi trường. `auto 0` và `auto 80` được dùng trong kiểm tra để chọn hai nhánh bằng cách đổi ngưỡng, số đo vẫn lấy từ DHT20 thật. Không diễn giải phép thử này thành việc nhiệt độ môi trường đã tăng hoặc giảm qua ngưỡng 30 °C.

Thư viện cục bộ: DHT20 0.2.2 và Adafruit NeoPixel 1.15.5, kèm giấy phép gốc. Mã nguồn không cần Wi-Fi hay mã truy cập CoreIOT. Khi nạp bài này, chương trình video 8 được thay thế. Các tệp nguồn gốc tại ~/Study/YoloUNO_PlatformIO không bị sửa.
