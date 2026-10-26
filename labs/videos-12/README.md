# Video 12: máy chủ web trên YoloUNO

Bài tích hợp từ các mô-đun LED, NeoPixel và DHT20 của video 9–11, thêm `mainserver.h/.cpp`. Bốn tác vụ là LED, NeoPixel, DHT20 và HTTP. `loop()` chỉ nhường thời gian xử lý.

## Biên dịch và nạp

```sh
pio run -e video12
pio run -e video12 -t upload
pio device monitor -b 115200
```

YoloUNO ESP32-S3, LED GPIO48, NeoPixel GPIO45, DHT20 SDA11/SCL12. Thư viện DHT20 0.2.2 và Adafruit NeoPixel 1.15.5 giữ cục bộ cùng giấy phép.

## Truy cập

Sau khi khởi động, kết nối điện thoại hoặc máy tính vào Wi-Fi `YoloUNO-Video12`, mật khẩu `yolouno-lab`, mở `http://192.168.4.1`.

Trang chính hiển thị nhiệt độ, độ ẩm, số thứ tự mẫu và hai nút bật/tắt. Trang `/settings` nhận tên mạng Wi-Fi 2,4 GHz và mật khẩu. Khi kết nối mạng nhà thành công, xem địa chỉ IP cấp cho bo qua USB hoặc trang trạng thái. Có thể mở trang web qua địa chỉ đó từ cùng mạng nhà.

Điểm phát của bo được giữ khi kết nối mạng nhà thành công (AP+STA). Sau 30 giây nếu không kết nối được, hoặc mất đường truyền, bo quay về AP. Nhấn giữ BOOT tối thiểu 80 ms để quay về AP, không cần RESET.

Thông tin Wi-Fi chỉ giữ trong RAM, không có trong mã nguồn. `WiFi.persistent(false)` tắt lưu cấu hình xuống flash. Nếu khởi động lại, cần nhập mạng lại. Điểm phát không cung cấp chức năng chuyển tiếp Internet cho điện thoại.

Có thể cấu hình khởi đầu qua USB bằng một dòng:

```text
wifi <ten-mang>|<mat-khau>
status
ap
```

Lệnh `ap` dùng để thử chuyển chế độ từ USB. Lệnh này khác với việc nhấn nút BOOT vật lý.

## Điều khiển qua tác vụ

Hàm xử lý `/toggle` chỉ cập nhật các biến `ledOn` và `neoOn` dưới khóa tương hỗ. Tác vụ LED và NeoPixel đọc biến rồi điều khiển phần cứng của mình. Máy chủ không ghi GPIO hoặc gọi NeoPixel trực tiếp, tránh xung đột với các tác vụ chớp từ bài trước.

Khi chạy bài web, `webControl=true` giữ đèn theo nút bấm, không tự chớp. Tác vụ kiểm tra lệnh khoảng mỗi 20 ms. Các trường `applied1`, `applied2` chỉ được cập nhật sau khi tác vụ đã gọi hàm xuất mức GPIO hoặc `show()`, không phải giá trị phản hồi từ cảm biến đo ánh sáng.

DHT20 đọc sau mỗi khoảng chờ 5 giây, trang chính lấy số đo và trạng thái mỗi 2 giây, trang cài đặt kiểm tra mạng mỗi 1,5 giây. Giá trị cảm biến lỗi hiển thị “Chưa có dữ liệu”, không được thay bằng dữ liệu giả.

| Tuyến HTTP | Phương thức | Chức năng |
| --- | --- | --- |
| `/` | GET | Trang chính |
| `/settings` | GET | Cài đặt Wi-Fi |
| `/sensors` | GET | Số đo thật, trạng thái hợp lệ, số mẫu |
| `/leds` | GET | Trạng thái yêu cầu và trạng thái tác vụ đã áp dụng |
| `/status` | GET | AP_ONLY, CONNECTING hoặc STA_CONNECTED và địa chỉ IP |
| `/toggle?led=1` hoặc `led=2` | POST | Đảo trạng thái đèn tương ứng |
| `/connect` | POST | Nhận ssid/password và lên lịch kết nối sau phản hồi |

Yêu cầu sai chỉ số LED hoặc thiếu tên mạng trả 400. Tuyến không tồn tại trả 404. Chương trình này chưa tích hợp MQTT, không gửi CoreIOT. Dữ liệu hiện trên web được phục vụ trực tiếp bởi bo.
