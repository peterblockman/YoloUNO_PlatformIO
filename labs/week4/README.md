# Tuần 4: semaphore và hàng đợi

## Chức năng

- BOOT GPIO0 gửi thông điệp sang tác vụ điều khiển LED GPIO48.
- Bốn chế độ: semaphore nhị phân, semaphore đếm, hàng đợi và thông điệp cấu trúc.
- `ButtonMessage` chứa `pressCount` và `durationMs`. Giữ trên 1 giây: LED sáng 2 giây; nhấn ngắn: chớp theo số thứ tự lần nhấn.
- DHT20 đọc mỗi 5 giây qua I²C GPIO11/12, gửi nhiệt độ/độ ẩm qua hàng đợi riêng sang tác vụ nhận.
- Nhật ký có thời gian, số mẫu và nhãn nguồn `BUTTON` hoặc `SYNTHETIC`.

## Biên dịch và nạp

```sh
pio run -e week4
pio run -e week4 -t upload
pio device monitor -b 115200
```

Chương trình khởi động ở chế độ `struct`. Các lệnh nhập vào cửa sổ nối tiếp:

```text
mode binary
mode counting
mode queue
mode struct
timeout 0
timeout 1000
stats
help
```

Đổi chế độ khi LED đã xử lý hết tín hiệu đang chờ. Mỗi lần đổi chế độ, số thứ tự lần nhấn bắt đầu lại từ 1. Thông điệp được gửi khi nhả nút để đo được thời gian giữ.

## Phép thử tạo đầu vào bằng chương trình

`pause` tạm dừng bên nhận, `burst N` tạo N lần nhấn giả lập, `run` tiếp tục xử lý. `press MS` tạo một thông điệp có thời gian giữ xác định. Các lệnh này được ghi nhãn `SYNTHETIC`, không phải bằng chứng nhấn tay.

Bộ đệm: semaphore nhị phân tối đa 1, semaphore đếm tối đa 10, hàng đợi nút tối đa 5 thông điệp. Tác vụ LED chờ nhận tối đa 20 ms để hỗ trợ đổi chế độ; tác vụ nhận cảm biến chờ bằng `portMAX_DELAY`.

## Phạm vi kiểm tra

Đã biên dịch/nạp trên YoloUNO, kiểm tra cảm biến và hàng đợi riêng; năm phép thử tự động về giới hạn bộ đệm, thời gian chờ và cấu trúc đạt. Đã quan sát nhấn ngắn làm LED chớp, giữ lâu làm LED sáng khoảng 2 giây. Các trường hợp nhấn nhanh/chậm đầy đủ theo slide và phép so sánh chỉ thay thời gian chờ còn cần bổ sung.

Các tệp cũ trong `src/` được giữ để tham khảo. Bộ lọc nguồn chỉ biên dịch `labs/week4/main.cpp` để tránh trùng hàm và đối tượng cảm biến.
