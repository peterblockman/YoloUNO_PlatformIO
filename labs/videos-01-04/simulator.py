"""Máy khách MQTT mô phỏng cho video 1–4, không đọc phần cứng."""
import argparse
import getpass
import json
import math
import os
import signal
import threading
import time
from datetime import datetime

import paho.mqtt.client as mqtt


def log(event, **values):
    print(json.dumps({'time': datetime.now().astimezone().isoformat(timespec='seconds'),
                      'event': event, **values}, ensure_ascii=False), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', type=int, choices=[1, 2], required=True)
    parser.add_argument('--host', default='app.coreiot.io')
    parser.add_argument('--port', type=int, default=1883)
    parser.add_argument('--interval', type=float, default=5)
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error('interval must be positive')
    access = os.environ.get('COREIOT_TOKEN') or getpass.getpass('Mã truy cập thiết bị: ')
    if not access.strip():
        parser.error('Mã truy cập không được rỗng')
    connected = threading.Event()
    stopped = threading.Event()
    state = {'led': False, 'fan': False}
    points = {1: (10.77, 106.66), 2: (10.88, 106.80)}
    client = mqtt.Client(client_id=f'video-device-{args.device}-{os.getpid()}')
    client.username_pw_set(access)
    access = None

    def publish_state():
        client.publish('v1/devices/me/attributes', json.dumps(state), qos=1)

    def on_connect(c, userdata, flags, rc):
        log('CONNECTED' if rc == 0 else 'CONNECT_FAILED', device=args.device, rc=rc)
        if rc == 0:
            c.subscribe('v1/devices/me/rpc/request/+', qos=1)
            publish_state()
            connected.set()

    def on_disconnect(c, userdata, rc):
        connected.clear()
        log('DISCONNECTED', device=args.device, rc=rc)

    def on_message(c, userdata, message):
        request_id = message.topic.rsplit('/', 1)[-1]
        try:
            payload = json.loads(message.payload)
            method = payload.get('method')
            params = payload.get('params')
            setters = {'setLedSwitchValue': 'led', 'setFanSwitchValue': 'fan'}
            getters = {'getLedSwitchValue': 'led', 'getFanSwitchValue': 'fan'}
            if method in getters:
                result = state[getters[method]]
            elif method in setters and type(params) is bool:
                state[setters[method]] = params
                result = params
                publish_state()
            else:
                result = {'success': False, 'error': 'Unsupported method or params'}
            c.publish('v1/devices/me/rpc/response/' + request_id,
                      json.dumps(result), qos=1)
            log('RPC', device=args.device, request_id=request_id,
                method=method, params=params, response=result, state=dict(state))
        except (ValueError, TypeError, AttributeError):
            log('RPC_INVALID_JSON', device=args.device)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()
    try:
        if not connected.wait(15):
            raise RuntimeError('Không kết nối được MQTT trong 15 giây')
        sequence = 0
        while not stopped.is_set():
            if connected.is_set():
                latitude, longitude = points[args.device]
                data = {'temperature': round(25 + args.device * 2 + math.sin(sequence / 4), 2),
                        'humidity': round(50 + args.device * 5 + 2 * math.cos(sequence / 4), 2),
                        'latitude': latitude, 'longitude': longitude,
                        'source': 'python_simulation', 'sequence': sequence}
                published = client.publish('v1/devices/me/telemetry', json.dumps(data), qos=1)
                published.wait_for_publish(timeout=5)
                if not published.is_published():
                    raise RuntimeError('Chưa nhận xác nhận publish trong 5 giây')
                log('PUBLISHED', device=args.device, qos=1, **data)
                sequence += 1
            stopped.wait(args.interval)
    finally:
        client.disconnect()
        client.loop_stop()


if __name__ == '__main__':
    main()
