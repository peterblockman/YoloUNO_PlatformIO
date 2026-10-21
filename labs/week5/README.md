# Tuần 5: máy chủ web và kết nối Wi-Fi

## Chức năng

- Đọc DHT20 trên I²C, SDA GPIO11 và SCL GPIO12.
- Trang chính hiển thị nhiệt độ, độ ẩm và điều khiển LED GPIO48 cùng NeoPixel GPIO45.
- AP `YoloUNO-Week5`, mật khẩu thực hành `yolouno-lab`, địa chỉ `192.168.4.1`.
- Trang `/settings` cấu hình Wi-Fi và CoreIOT. Trang `/wifi` quét danh sách mạng, chọn mạng và kết nối.
- Giữ AP khi kết nối STA. Thời gian chờ 30 giây, thất bại quay về AP. BOOT có nhánh trở về AP.
- Semaphore đánh thức tác vụ MQTT sau khi Wi-Fi kết nối. MQTT gửi dữ liệu và xử lý RPC `setLedSwitchValue` với tham số boolean khi đã cấu hình mã truy cập.
- Mật khẩu Wi-Fi và mã truy cập chỉ nhận khi chạy, giữ trong RAM, không in vào nhật ký.

## Biên dịch và nạp

```sh
pio run -e week5
pio run -e week5 -t upload
pio device monitor -b 115200
```

Sau khi nạp, nối vào AP của bo và mở `http://192.168.4.1/wifi`. Chọn mạng, nhập mật khẩu và nhấn Kết nối. Có thể tiếp tục dùng IP STA được hiển thị trên trang hoặc nhật ký.

## Phạm vi kiểm tra

Đã kiểm tra trên YoloUNO: DHT20, HTTP điều khiển hai đèn, kết nối STA, semaphore, mạng không tồn tại quay về AP, quét danh sách và kết nối mạng được chọn từ trình duyệt. Chức năng CoreIOT/RPC, BOOT trên bản tuần 5 và máy khách truy cập trực tiếp AP còn cần kiểm tra bổ sung. Chưa có LCD nên chưa thực hiện bài hiển thị IP lên LCD.

Các tệp cũ trong `src/` được giữ lại để tham khảo. Bộ lọc nguồn chỉ biên dịch `labs/week5/main.cpp`, tránh trùng `setup()` và máy chủ cũ cùng cổng 80.
