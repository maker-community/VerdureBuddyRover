# ESP32-C3 上位机串口协议

当前固件通过 UART1 与上位机通信：TX=GPIO20，RX=GPIO21，波特率 115200，8N1。USB CDC `Serial` 同时支持调试输入输出，但正式上位机建议使用 UART1。

协议为行式文本：上位机发送一行命令，以 `\r\n` 或 `\n` 结束；固件返回文本或 JSON。

## 命令

| 命令 | 示例 | 返回 |
|---|---|---|
| `help` / `?` | `help` | 命令列表 |
| `ping` | `ping` | `OK ping` |
| `status` | `status` | 状态 JSON |
| `battery` | `battery` | `battery level=87 charging=1 discharging=0` |
| `motor` | `motor 0 fwd 200` | `OK motor 0` |
| `key` | `key 1 1` | `OK key 1` |
| `rgb` | `rgb 255 0 0` | `OK rgb` |

## `status` JSON

示例：

```json
{
  "battery": {
    "level": 87,
    "charging": 1,
    "discharging": 0,
    "mv": 4100,
    "ma": 120
  },
  "motor": {
    "m0": 200,
    "m1": -160
  },
  "keys": {
    "k1": 0
  },
  "rgb": [255, 0, 0],
  "ble": {
    "connected": 1
  }
}
```

字段约定：

- `battery.level`: 电量百分比，读取失败时固件归零。
- `battery.charging` / `battery.discharging`: 0 或 1，通过平均电流阈值判断。
- `motor.m0` / `motor.m1`: 当前电机速度，范围 `-255..255`。
- `keys.k1`: 当前按键稳定状态，1 表示按下。
- `rgb`: RGB5050 三通道亮度，范围 `0..255`。
- `ble.connected`: BLE 键盘是否已连接。

## 电机控制

```text
motor <0|1> <fwd|rev|stop> [0-255]
```

示例：

```text
motor 0 fwd 200
motor 1 rev 160
motor 0 stop
```

固件会把速度限制在 `-255..255`。`stop` 会忽略速度参数并停止对应电机。

## 主动事件

按键状态变化时，固件会主动推送事件：

```text
EVT key k1 DOWN
EVT key k1 UP
```

上位机工具应能同时处理命令响应和主动事件；建议读取串口时按行分发，JSON 行走状态解析，`EVT` 行走事件处理。

## Python 最小示例

```python
import json
import time

import serial

ser = serial.Serial("COM5", 115200, timeout=0.5)


def get_status():
    ser.write(b"status\r\n")
    while True:
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            return None
        if line.startswith("{"):
            return json.loads(line)


while True:
    status = get_status()
    if status:
        battery = status["battery"]
        print(f"电量={battery['level']}% 充电={battery['charging']} 放电={battery['discharging']}")
    time.sleep(5)
```