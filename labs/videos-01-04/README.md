# Thực hành video 1–4

Hai máy khách Python mô phỏng hai thiết bị IoT, gửi dữ liệu MQTT mỗi 5 giây. Giá trị nhiệt độ, độ ẩm và tọa độ đều phục vụ minh họa, không phải số đo từ DHT20 hay GPS.

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python simulator.py --device 1
```

Nhập mã truy cập của `Video-Device-1` khi được hỏi. Mở một cửa sổ khác và chạy với `--device 2`, dùng mã của `Video-Device-2`. Mã không được in ra nhật ký hoặc lưu trong mã nguồn. Nhấn Ctrl+C để dừng.

Dữ liệu gửi lên `v1/devices/me/telemetry` có temperature, humidity, latitude, longitude, source và sequence. Trường source luôn là `python_simulation`. Mỗi thiết bị có trạng thái led và fan độc lập.

Các phương thức RPC gồm getLedSwitchValue, setLedSwitchValue, getFanSwitchValue và setFanSwitchValue. Các lệnh đặt nhận tham số boolean. Chương trình cập nhật thuộc tính thiết bị và trả phản hồi đúng request_id. LED và quạt trong bài này chỉ là trạng thái mô phỏng.

Tài liệu: https://pypi.org/project/paho-mqtt/1.6.1/
