# Firmware

这里存放 VerdureBuddyRover 的 MCU 固件。每个固件使用独立目录，目录名尽量表达目标板卡或功能；Arduino 草图目录必须与主 `.ino` 文件同名。

## 当前固件

| 目录 | 平台 | 说明 |
|---|---|---|
| [esp32c3_kb_motor](esp32c3_kb_motor) | ESP32-C3 SuperMini / Arduino | 电池计量、双电机、BLE 键盘、RGB、UART 上位机协议 |

## 命名建议

- `esp32c3_kb_motor`: 当前综合固件，偏硬件 bring-up 和调试。
- `esp32c3_rover_main`: 后续可以演进为正式小车主控固件。
- `esp32c3_sensor_node`: 如果未来拆出独立传感器节点，可单独建目录。

避免把多个无关实验草图直接堆在同一个目录；需要复用的逻辑后续可以抽到 `firmware/common/`。