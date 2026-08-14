# ESP32-C3 上位机串口协议

当前固件通过 UART1 与上位机通信：TX=GPIO20，RX=GPIO21，波特率 115200，8N1。USB CDC `Serial` 同时支持调试输入输出，但正式上位机建议使用 UART1。

协议为行式文本：上位机发送一行命令，以 `\r\n` 或 `\n` 结束；固件返回文本或 JSON。

## MCP 控制原则

上位机 MCP 工具只需要发送一条完整动作命令，并等待这一条命令对应的 `OK` 或 `ERR`。不要为一次底盘动作分别拼接两条低层 `motor` 命令；左右轮应使用 `drive` 一次性下发。推荐的 MCP 映射如下：

| MCP 工具 | 串口命令 |
|---|---|
| `self.chassis.go_forward(speed)` | `forward <speed>`，固件限制最高 40 |
| `self.chassis.go_back(speed)` | `back <speed>`，固件限制最高 40 |
| `self.chassis.turn_left(speed)` | `turn left <speed>`，固件限制最高 40 |
| `self.chassis.turn_right(speed)` | `turn right <speed>`，固件限制最高 40 |
| `self.chassis.stop()` | `stop` |
| `self.chassis.get_status()` | `status` |
| `self.led.set_color(red, green, blue)` | `rgb <red> <green> <blue>` |
| `self.led.off()` | `led off` |

速度统一为 `0..100` 百分比，但桌面安全上限固定为 `40`。即使上位机误发 `100`，固件也只会以 `40` 执行。固件内部再换算为 DRV8833 的 `0..255` PWM。

## 命令

| 命令 | 示例 | 返回 |
|---|---|---|
| `help` / `?` | `help` | 命令列表 |
| `ping` | `ping` | `OK ping` |
| `status` | `status` | 状态 JSON |
| `battery` | `battery` | `battery level=87 charging=1 discharging=0` |
| `drive` | `drive 80 80` | `OK drive left=80 right=80` |
| `forward` | `forward 80` | `OK drive left=80 right=80` |
| `back` | `back 60` | `OK drive left=-60 right=-60` |
| `turn` / `spin` | `turn left 30` | `OK drive left=-30 right=30` |
| `stop` | `stop` | `OK drive left=0 right=0` |
| `motor` | `motor 0 fwd 200` | `OK motor 0` |
| `led` | `led on` / `led off` | `OK led state=1` / `OK led state=0` |
| `led emotion` | `led emotion happy` | `OK led emotion=happy mode=breathe` |
| `led breathe` | `led breathe 0 120 255 1600` | `OK led mode=breathe` |
| `led blink` | `led blink 255 0 0 500` | `OK led mode=blink` |
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
    "m0": 40,
    "m1": -40
  },
  "keys": {
    "k1": 0
  },
  "rgb": [255, 0, 0],
  "led": {
    "mode": "solid",
    "period_ms": 1600
  },
  "speed_limit": 40,
  "ble": {
    "connected": 1
  }
}
```

字段约定：

- `battery.level`: 电量百分比，读取失败时固件归零。
- `battery.charging` / `battery.discharging`: 0 或 1，通过平均电流阈值判断。
- `motor.m0` / `motor.m1`: 当前左右轮速度百分比，范围 `-100..100`。
- `speed_limit`: 当前固件实际允许的最高速度百分比，桌面版本为 `40`。
- `keys.k1`: 当前按键稳定状态，1 表示按下。
- `rgb`: RGB5050 三通道亮度，范围 `0..255`。
- `ble.connected`: BLE 键盘是否已连接。

## 底盘控制

### 原子左右轮控制

```text
drive <left> <right>
```

`left` 和 `right` 范围都是 `-100..100`：正数前进，负数后退，0 停止。该命令在固件内连续更新两个电机，并返回统一状态：

```text
drive 80 80
OK drive left=80 right=80
```

### 语义化动作

```text
forward [speed]
back [speed]
turn <left|right> [speed]
stop
```

`turn left` 和 `turn right` 会让左右轮反向转动，实现原地左转/右转。`spin` 是 `turn` 的同义词。省略速度时，前进/后退默认 `80`，转向默认 `60`，但最终都会被桌面安全上限 `40` 截断。

## 低层电机控制

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

该命令仅用于底层调试，不建议 MCP 使用。MCP 应使用上面的 `drive` 或语义化动作命令。

## 灯光控制

```text
led on
led off
rgb <r> <g> <b>
```

`led on` 设置白光 `255,255,255`；需要指定颜色时使用 `rgb`，三个通道范围都是 `0..255`。

### 动画与情绪预设

```text
led breathe <r> <g> <b> [period_ms]
led blink <r> <g> <b> [period_ms]
led emotion <happy|listening|thinking|charging|warning|error|working|off>
```

这些动画在固件主循环中非阻塞运行，不会阻塞电机、按键或串口。情绪预设：

| 情绪 | 颜色/模式 |
|---|---|
| `happy` | 绿色呼吸 |
| `listening` | 蓝色呼吸 |
| `thinking` | 紫色呼吸 |
| `charging` | 青色呼吸 |
| `working` | 橙色呼吸 |
| `warning` | 橙色闪烁 |
| `error` | 红色闪烁 |

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