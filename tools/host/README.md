# Host Tools

这里预留上位机调试工具。当前固件已经提供 UART 行式协议，后续工具建议都复用 [../../docs/serial-protocol.md](../../docs/serial-protocol.md) 的协议定义。

## 建议演进顺序

1. `serial_console/`: Python + pyserial 的最小命令行工具，支持 `status`、`battery`、`motor`、`rgb` 和事件监听。
2. `protocol_tests/`: 用串口或模拟串口做协议回归测试，避免固件改动破坏上位机解析。
3. `dashboard/`: GUI 或 Web 调试面板，用于显示电量、电机速度、按键事件、BLE 状态。

## 约定

- 工具代码不要硬编码太多硬件细节；引脚和电气说明放在 `hardware/`。
- 协议新增字段时，先更新 `docs/serial-protocol.md`，再更新工具解析逻辑。
- 如果使用 Python，后续可在本目录添加 `requirements.txt` 或 `pyproject.toml`。