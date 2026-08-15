# ESP32-C3 SuperMini 综合固件

功能：BQ27220 电池计量 + DRV8833 双电机 + BLE 蓝牙键盘 + RGB5050 指示灯 + 上位机 UART 行式协议。串口协议参考 xiaozhi-esp32 `atom-echos3r.cc` 的行式文本命令和 `status` JSON 风格。

## 依赖库

| 库 | 用途 | 当前状态 |
|---|---|---|
| `kode_bq27220` | BQ27220 电量计 | 本机位于 `C:\arduino\Arduino\libraries` |
| `HijelHID_BLEKeyboard` | BLE HID 键盘 | 必需；依赖 `NimBLE-Arduino` |

> 项目不要求修改第三方库源码。之前的本地补丁只用于定位崩溃原因；当前固件已移除旧 BLE 键盘库代码，统一使用 `HijelHID_BLEKeyboard`。细节见 [../../docs/ble-keyboard-library.md](../../docs/ble-keyboard-library.md)。

## 硬件接线

| 功能 | 引脚 | 备注 |
|---|---|---|
| BQ27220 | SDA=GPIO5, SCL=GPIO6 | 3.3V/GND |
| DRV8833 电机A | AIN1=GPIO0, AIN2=GPIO1 | VCC/VMOT 接电机电源，需共地 |
| DRV8833 电机B | BIN1=GPIO4, BIN2=GPIO7 | N20 3-6V 电机可用 15kHz PWM 降低噪声 |
| 按键 x1 | GPIO10 -> GND | 内部上拉，按下为低，当前映射 Enter；电机干扰明显时可在按键两端并联 0.1 µF 陶瓷电容 |
| RGB5050 | R=GPIO2, G=GPIO3, B=GPIO9 | 共阴接 GND，三路需串限流电阻 |
| 上位机串口 | UART1 TX=GPIO20, RX=GPIO21 | 115200 8N1，需 USB-TTL 共地 |

USB CDC `Serial` 主要用于烧录和调试；上位机正式协议走 UART1 `Serial1`。

## Arduino IDE 设置

- 开发板：`ESP32C3 Dev Module`，或对应 ESP32-C3 SuperMini 定义。
- USB CDC on Boot：`Enabled`。
- 主文件：[esp32c3_kb_motor.ino](esp32c3_kb_motor.ino)。

命令行编译示例：

```powershell
$env:ARDUINO_DIRECTORIES_DATA="C:\arduino\Arduino15"
$env:ARDUINO_DIRECTORIES_USER="C:\arduino\Arduino"
& "C:\Users\张广建\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile `
  --fqbn esp32:esp32:esp32c3 `
  --build-path "C:\arduino\build_verdurebuddy_esp32c3" `
  "firmware\esp32c3_kb_motor"
```

Windows 用户名包含中文时，建议把 Arduino 数据目录和构建目录放到纯英文路径，避免 ESP32 工具链链接阶段无法打开输出文件。

## 串口协议速查

详见 [../../docs/serial-protocol.md](../../docs/serial-protocol.md)。常用命令：

```text
help
ping
status
battery
forward 80
back 60
turn left 30
drive -60 60
limit 80
stop
led on
led emotion happy
led breathe 0 120 255 1600
key 1 1
rgb 255 0 0
```

小智 MCP 的对接规则见 [../../docs/xiaozhi-mcp-serial-integration.md](../../docs/xiaozhi-mcp-serial-integration.md)。MCP 控制底盘时优先使用 `drive` 或 `forward/back/turn/stop`，不要拆成两条低层 `motor` 命令。

固件默认速度和最高限速均为 100%，可用 `limit <0-100>` 临时调整；低层 `motor` 命令直接使用 `0..255` PWM，仅用于单电机调试。RGB 灯支持常亮、呼吸、闪烁和 `happy/listening/thinking/charging/warning/error/working` 情绪预设。

BLE 键盘在开机后会以 `Verdure Buddy Rover` 启动广播，手机或电脑负责配对和自动回连。BLE 初始化不依赖打开 USB 串口监视器；固件在未连接且广播意外停止时会每 1 秒自动恢复广播。设备名变化后，需删除系统里的旧配对记录并重新配对；可用 `status` 查看 `ble.connected` 是否为 `1`。

## 历史问题

### BLE 键盘连接崩溃

症状：蓝牙设备能被枚举到，但手机或电脑一连接就失败；串口可能出现：

```text
Guru Meditation Error: Core  0 panic'ed (Load access fault). Exception was unhandled.
MEPC: 0x42001bec   MCAUSE: 0x00000005   MTVAL: 0x0000002c
```

根因：`ESP32 BLE Keyboard` 库的 `BleKeyboard.cpp` 中，`onConnect()` / `onDisconnect()` 调用 `getDescriptorByUUID(BLEUUID(0x2902))` 可能返回空指针，未判空就调用 `desc->setNotifications(...)`，导致空指针解引用。

曾经通过在库源码里判空验证了问题点：

```cpp
BLE2902* desc = (BLE2902*)this->inputKeyboard->getDescriptorByUUID(BLEUUID(0x2902));
if (desc) desc->setNotifications(true);
```

`onDisconnect()` 中设置 `false` 的位置也存在同类风险。但这不是项目方案，因为库管理器更新会覆盖修改，也不利于其他机器复现。

当前固件已改用 `HijelHID_BLEKeyboard`，不再包含旧 `ESP32 BLE Keyboard` 代码路径，也不需要修改第三方库源码。