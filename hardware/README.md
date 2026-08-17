# Hardware

这里存放 VerdureBuddyRover 的硬件资料。当前固件对应的硬件连接如下，后续可以按硬件版本继续拆分。

## 外壳 (Enclosure)

圆滚滚的可爱小车外壳，可 3D 打印，渲染图如下：

![正面](enclosure/images/face.png)

![背面](enclosure/images/back.png)

模型源文件与 3D 打印文件见 [enclosure](enclosure) 目录。

## 当前接线

| 模块 | ESP32-C3 引脚 | 说明 |
|---|---|---|
| BQ27220 SDA | GPIO5 | I2C 数据线 |
| BQ27220 SCL | GPIO6 | I2C 时钟线 |
| DRV8833 AIN1 | GPIO0 | 电机 A 方向/PWM |
| DRV8833 AIN2 | GPIO1 | 电机 A 方向/PWM |
| DRV8833 BIN1 | GPIO4 | 电机 B 方向/PWM |
| DRV8833 BIN2 | GPIO7 | 电机 B 方向/PWM |
| 按键 | GPIO10 | 接 GND，内部上拉，按下为低 |
| RGB5050 R | GPIO2 | 共阴 RGB，需限流电阻 |
| RGB5050 G | GPIO3 | 共阴 RGB，需限流电阻 |
| RGB5050 B | GPIO9 | 共阴 RGB，需限流电阻 |
| UART1 TX | GPIO20 | 接上位机 USB-TTL RX |
| UART1 RX | GPIO21 | 接上位机 USB-TTL TX |

## 后续建议

- `hardware/rev-a/`: 第一版面包板或洞洞板接线记录。
- `hardware/rev-b/`: PCB 原理图、Gerber、BOM、装配说明。
- `hardware/enclosure/`: 外壳、底盘、3D 打印文件。
- `hardware/assets/`: 接线图、实物照片、示波器截图等图片资料。

硬件改动会直接影响 [firmware/esp32c3_kb_motor/esp32c3_kb_motor.ino](../firmware/esp32c3_kb_motor/esp32c3_kb_motor.ino) 中的引脚配置，建议每次调整都同步更新这里。