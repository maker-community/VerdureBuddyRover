# 小智 MCP 串口对接

小智 `atom-echos3r` 上位机通过 UART1 控制 ESP32-C3 小车板。建议 MCP 工具只负责生成高层动作，串口层只发送一行命令并等待一行结果。

## MCP 到串口映射

| MCP 工具 | 参数 | 串口命令 |
|---|---|---|
| `self.chassis.go_forward` | `speed=0..100` | `forward <speed>`，固件最高执行 40 |
| `self.chassis.go_back` | `speed=0..100` | `back <speed>`，固件最高执行 40 |
| `self.chassis.turn_left` | `speed=0..100` | `turn left <speed>`，固件最高执行 40 |
| `self.chassis.turn_right` | `speed=0..100` | `turn right <speed>`，固件最高执行 40 |
| `self.chassis.stop` | 无 | `stop` |
| `self.chassis.get_status` | 无 | `status` |
| `self.led.set_color` | `red/green/blue=0..255` | `rgb <red> <green> <blue>` |
| `self.led.off` | 无 | `led off` |
| `self.led.set_emotion` | `emotion` | `led emotion <emotion>` |

## 发送规则

1. 命令必须以 `\r\n` 结束。
2. 一次动作只发送一行，不要把一次底盘动作拆成两个 `motor` 命令。
3. 发送后读取完整一行，等待 `OK ...` 或 `ERR ...`。
4. `EVT ...` 和 `battery ...` 属于异步事件/状态行，不要当作动作确认。
5. UART1 当前不会主动周期上报电量，只有 `battery` 或 `status` 查询会返回电池信息。
6. `stop` 是幂等的，可在动作超时或语音指令不明确时重复发送。
7. 原地转圈使用 `turn left <speed>` 或 `turn right <speed>`，左右电机方向相反。

## 上位机替换示例

原来小智上位机的 `DriveMotors(left, right)` 可以改为发送一条 `drive`：

```cpp
void DriveMotors(int left, int right) {
    left = std::max(-40, std::min(40, left));
    right = std::max(-40, std::min(40, right));

    char buf[48];
    snprintf(buf, sizeof(buf), "drive %d %d", left, right);
    serial_.SendLine(buf);
}
```

如果保留 MCP 的语义化工具，也可以分别发送 `forward`、`back`、`turn left`、`turn right` 和 `stop`。推荐语义化命令，因为它们更容易在 MCP 层做参数校验。

## 响应示例

```text
drive 80 80
OK drive left=80 right=80

turn left 60
OK drive left=-60 right=60

stop
OK drive left=0 right=0
```

状态查询返回的电机值也是 `-100..100` 百分比，不再暴露内部 `0..255` PWM：

```json
{"battery":{"level":87,"charging":0,"discharging":1,"mv":4100,"ma":-120},"motor":{"m0":80,"m1":80},"keys":{"k1":0},"rgb":[0,0,0],"ble":{"connected":1}}
```

## 为什么原来的方式容易失控

- 一次底盘动作拆成两条 `motor` 命令，中间缺少统一动作边界。
- 左右轮同时发送时，上位机和下位机都只能依赖串口行缓冲时序。
- USB 和 UART1 原先共用接收缓冲区，半行输入可能互相污染。
- UART1 原先会周期插入电量行，简单的上位机“发命令后读一行”可能读到电量而不是 ACK。

当前固件已经分别处理 USB/UART1 输入，并让 UART1 保持请求/响应式控制。