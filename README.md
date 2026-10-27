# Thực hành Lập trình ứng dụng IoT trên YoloUNO

Kho mã nguồn cho các bài thực hành trên YoloUNO ESP32-S3 và cảm biến DHT20, sử dụng PlatformIO và FreeRTOS. Các bài được lưu theo nhánh. Bảng dưới đây dẫn trực tiếp tới phần mã tương ứng với từng bài trong báo cáo cá nhân.

## Tra cứu theo mẫu báo cáo cá nhân

| Bài trong báo cáo | Nhánh | Mã nguồn cần xem | Nội dung đối chiếu |
| --- | --- | --- | --- |
| **Bài 1. CoreIOT** | [Video_08](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_08) | [labs/videos-08/src/main.cpp](https://github.com/peterblockman/YoloUNO_PlatformIO/blob/Video_08/labs/videos-08/src/main.cpp) | Gửi số đo DHT20 qua MQTT, cập nhật thuộc tính và xử lý lệnh điều khiển từ CoreIOT. Việc tạo tài khoản, thiết bị và bảng điều khiển được minh chứng bằng ảnh trong báo cáo. |
| **Bài 2. LED Blinky** | [Video_05_07](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_05_07) | [labs/videos-05-07/src/main.cpp](https://github.com/peterblockman/YoloUNO_PlatformIO/blob/Video_05_07/labs/videos-05-07/src/main.cpp) | Cấu hình `video6`, tác vụ LED GPIO48 sáng 2 giây, tắt 2 giây và ghi trạng thái qua USB. |
| **Bài 3. I²C và DHT20** | [Video_05_07](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_05_07) | [labs/videos-05-07/src/main.cpp](https://github.com/peterblockman/YoloUNO_PlatformIO/blob/Video_05_07/labs/videos-05-07/src/main.cpp) | Cấu hình `video7`, đọc DHT20 qua SDA11/SCL12. Kết quả quét địa chỉ `0x38` nằm trong báo cáo, đoạn quét được trình bày riêng với phần đọc cảm biến. |
| **Bài 4. Semaphore** | [RTOS_Project_Week5](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/RTOS_Project_Week5) | [labs/week5/main.cpp](https://github.com/peterblockman/YoloUNO_PlatformIO/blob/RTOS_Project_Week5/labs/week5/main.cpp) | Cờ hiệu nhị phân `networkReady` đồng bộ tác vụ kết nối Wi-Fi và tác vụ MQTT, có nhật ký `give`/`take`. Bài mở rộng các loại semaphore và hàng đợi nằm ở nhánh tuần 4 bên dưới. |
| **Bài 5. Webserver** | [Video_12](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_12) | [labs/videos-12/src/mainserver.cpp](https://github.com/peterblockman/YoloUNO_PlatformIO/blob/Video_12/labs/videos-12/src/mainserver.cpp) | Máy chủ web trên bo, hiển thị DHT20, điều khiển LED và NeoPixel, cấu hình Wi-Fi, hỗ trợ AP/STA. |

Mỗi thư mục bài có README riêng hướng dẫn biên dịch, nạp và sử dụng. Với các nhánh tuần 4–5, cấu hình `build_src_filter` chọn mã trong `labs/`. Với các nhánh video, cần mở đúng thư mục con chứa `platformio.ini`, không biên dịch thư mục gốc thay cho bài tương ứng.

## Bản đồ nhánh theo video

| Video | Nhánh | Thư mục bài và hướng dẫn | Nội dung |
| --- | --- | --- | --- |
| 1–4 | [Video_01_04](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_01_04) | [labs/videos-01-04](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_01_04/labs/videos-01-04) | Hai thiết bị mô phỏng bằng Python, MQTT và điều khiển trạng thái qua CoreIOT. Dữ liệu được tạo bởi chương trình, không phải số đo từ phần cứng. |
| 5–7 | [Video_05_07](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_05_07) | [labs/videos-05-07](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_05_07/labs/videos-05-07) | Thiết lập PlatformIO, LED với FreeRTOS (`video6`), bổ sung DHT20 (`video7`). |
| 8 | [Video_08](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_08) | [labs/videos-08](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_08/labs/videos-08) | Số đo thật từ DHT20 lên CoreIOT, lệnh điều khiển LED và xử lý kết nối lại. |
| 9–11 | [Video_09_11](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_09_11) | [labs/videos-09-11](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_09_11/labs/videos-09-11) | Tách mô-đun LED (`video9`), NeoPixel (`video10`) và DHT20 (`video11`), chia sẻ dữ liệu giữa các tác vụ. |
| 12 | [Video_12](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_12) | [labs/videos-12](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/Video_12/labs/videos-12) | Trang web nội bộ hiển thị cảm biến và điều khiển hai đèn. |

Video 13–16 chưa có nhánh thực hành được công bố trong kho này. Số video và số tuần là hai cách phân chia nội dung khác nhau, không tương ứng một đối một.

## Bản đồ nhánh theo tuần

| Tuần | Nhánh | Mã nguồn và hướng dẫn | Nội dung |
| --- | --- | --- | --- |
| 4 | [RTOS_Project_Week4](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/RTOS_Project_Week4) | [labs/week4](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/RTOS_Project_Week4/labs/week4) | Semaphore nhị phân, semaphore đếm, hàng đợi và thông điệp cấu trúc. Nút BOOT gửi yêu cầu sang tác vụ LED, DHT20 dùng hàng đợi riêng. |
| 5 | [RTOS_Project_Week5](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/RTOS_Project_Week5) | [labs/week5](https://github.com/peterblockman/YoloUNO_PlatformIO/tree/RTOS_Project_Week5/labs/week5) | Máy chủ web, quét và chọn Wi-Fi tại `/wifi`, cấu hình tại `/settings`, cảm biến, điều khiển hai đèn và CoreIOT. |

Phần hiển thị IP lên LCD chưa thực hiện vì phần cứng sử dụng chỉ có bo và DHT20. Các phép thử tạo đầu vào bằng chương trình trong bài tuần 4 được gắn nhãn `SYNTHETIC`, phân biệt với thao tác nhấn nút thật.

## Video minh chứng

| Nội dung | Video |
| --- | --- |
| Nút BOOT và LED, bài tuần 4 | [Xem trên Google Drive](https://drive.google.com/file/d/17XFbBJY4v1LkyMknhb1BlLnMzQ8ptp2D/view) |
| LED sáng/tắt mỗi 2 giây, cấu hình `video6`, Bài 2 trong báo cáo | [Xem trên Google Drive](https://drive.google.com/file/d/1AA2C6cQsAqr_m6Cg_eTbboYJ3aArfnBy/view) |
| Thực hành video 8, CoreIOT và bo | [Xem trên Google Drive](https://drive.google.com/file/d/1PNDjt2s6Ht9GWmpgGX8iGQ1d_LXWwuZf/view) |
| LED và NeoPixel, nhóm video 9–11 | [Xem trên Google Drive](https://drive.google.com/file/d/1rimcpSf00p_PzPmq4xw6GXnAqgeUbpAv/view) |
| Bật/tắt LED từ Webserver nội bộ, Bài 5 trong báo cáo | [Xem trên Google Drive](https://drive.google.com/file/d/1e3PcDJYo4nVDjj-uN3YaStnOvbopGDZ9/view) |

## Phần cứng và môi trường

- YoloUNO ESP32-S3, kết nối USB với macOS M1.
- LED đơn GPIO48, NeoPixel GPIO45, nút BOOT GPIO0.
- DHT20 qua I²C, SDA GPIO11, SCL GPIO12, địa chỉ `0x38`.
- PlatformIO, Arduino và FreeRTOS. Nhật ký nối tiếp dùng tốc độ `115200` baud.

## Cách chạy một bài

Ví dụ chạy Webserver của video 12 trong một bản sao riêng của nhánh đó:

```sh
git clone --branch Video_12 --single-branch https://github.com/peterblockman/YoloUNO_PlatformIO.git YoloUNO-Video12
cd YoloUNO-Video12/labs/videos-12
pio run -e video12
pio run -e video12 -t upload
pio device monitor -b 115200
```

Chọn cấu hình theo bài cần chạy:

| Nhánh | Thư mục chạy lệnh | Cấu hình PlatformIO |
| --- | --- | --- |
| `RTOS_Project_Week4` | Gốc của nhánh | `week4` |
| `RTOS_Project_Week5` | Gốc của nhánh | `week5` |
| `Video_05_07` | `labs/videos-05-07` | `video6` hoặc `video7` |
| `Video_08` | `labs/videos-08` | `video8` |
| `Video_09_11` | `labs/videos-09-11` | `video9`, `video10` hoặc `video11` |
| `Video_12` | `labs/videos-12` | `video12` |

Nhánh `Video_01_04` chạy bằng Python, xem hướng dẫn trong thư mục bài. Lệnh `pio run` chỉ biên dịch, cần thêm `-t upload` để nạp. Mỗi lần nạp thay chương trình đang chạy trên bo.

Thông tin Wi-Fi và mã truy cập CoreIOT được nhập khi chạy theo hướng dẫn từng bài. Không đưa thông tin truy cập thật vào mã nguồn hoặc nhật ký công khai.

## Tài liệu tham khảo

- Các video và slide học phần Lập trình ứng dụng IoT của giảng viên.
- [PlatformIO cho Espressif32](https://registry.platformio.org/platforms/platformio/espressif32).
- [Cấu hình dự án PlatformIO](https://docs.platformio.org/en/latest/projectconf/index.html).
- Các thư viện và giấy phép đi kèm từng bài, gồm DHT20, PubSubClient, ArduinoJson và Adafruit NeoPixel.

Mã nguồn được phát triển từ dự án và ví dụ học phần. ChatGPT/Codex hỗ trợ tổ chức mã, kiểm tra và biên tập tài liệu. Học viên thực hiện thao tác phần cứng và cung cấp ảnh, video minh chứng.
