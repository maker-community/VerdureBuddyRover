# 项目目录规划

本仓库按“硬件资料、固件、上位机工具、通用文档”拆分，避免后续功能增长时根目录混乱。

## 顶层目录

| 目录 | 用途 | 何时添加内容 |
|---|---|---|
| `firmware/` | ESP32、Arduino、未来其他 MCU 固件 | 新增板卡固件、传感器节点、正式 rover 主控固件 |
| `hardware/` | 接线、BOM、原理图、PCB、外壳结构 | 接线变更、硬件版本归档、物料清单 |
| `docs/` | 协议、调试说明、架构说明 | 需要给人读的设计和调试资料 |
| `tools/host/` | 上位机调试工具 | Python/Qt/Web/串口可视化工具 |
| `scripts/` | 自动化脚本 | 编译、烧录、抓日志、格式化等脚本 |

## 固件目录规则

Arduino 草图目录必须和主 `.ino` 文件同名，例如：

```text
firmware/esp32c3_kb_motor/
  esp32c3_kb_motor.ino
  README.md
```

后续如果固件变复杂，可以逐步演进为：

```text
firmware/esp32c3_rover_main/
  esp32c3_rover_main.ino
  battery.ino
  motors.ino
  protocol.ino
  pins.h
  README.md
```

如果准备迁移到 PlatformIO 或 ESP-IDF，建议新增平行目录，不要直接覆盖已可工作的 Arduino 版本。

## 上位机工具规划

上位机调试工具建议从 `tools/host/` 起步：

```text
tools/host/
  README.md
  serial_console/      # 最小串口命令行工具
  dashboard/           # 后续 GUI 或 Web 可视化面板
  protocol_tests/      # 协议回归测试或模拟器
```

协议定义统一放在 [serial-protocol.md](serial-protocol.md)，工具代码不要各自复制一份协议说明。