# BLE 键盘库选择与崩溃原因

当前固件需要 ESP32-C3 模拟 BLE HID 键盘，只用到很小一部分能力：

- `begin()`：启动 BLE HID 键盘广播。
- `isPaired()`：判断主机是否已连接并完成配对。
- `press(hidCode)` / `release(hidCode)`：发送按下和抬起事件。

## 结论

不要把“修改第三方库源码”作为项目方案。当前固件只使用独立 BLE HID 键盘库 `HijelHID_BLEKeyboard`，它基于 `NimBLE-Arduino 2.3.8+`，可以覆盖当前需要的 `begin()`、配对状态、`press()`、`release()` 能力。

项目已经移除旧的 `ESP32 BLE Keyboard` 代码路径，不再提供兼容开关。需要安装 Arduino 库 `HijelHID_BLEKeyboard`，库管理器会自动安装依赖 `NimBLE-Arduino`。

## 崩溃原因

你之前遇到的崩溃点在 `ESP32 BLE Keyboard` 的旧 Bluedroid 分支，不是电机、电池或按键代码导致的。

连接发生时，库会进入 `onConnect()` / `onDisconnect()`，尝试通过 `getDescriptorByUUID(BLEUUID(0x2902))` 获取 `BLE2902` 描述符。这个描述符是 BLE Client Characteristic Configuration Descriptor，用来控制 notify/indicate。某些 ESP32 Arduino core、BLE 库版本或 HID report 初始化组合下，这个调用可能返回空指针。

旧库代码没有判空，继续执行类似下面的逻辑：

```cpp
BLE2902* desc = (BLE2902*)inputKeyboard->getDescriptorByUUID(BLEUUID(0x2902));
desc->setNotifications(true);
```

当 `desc == nullptr` 时，`desc->setNotifications(true)` 就会空指针解引用，ESP32-C3 上表现为 `Load access fault`，常见现象是“蓝牙能搜到，但一连接就重启或断开”。

本地给库源码加 `if (desc)` 只能证明问题点，不适合作为项目方案；当前仓库不再依赖这条代码路径。

## 为什么建议 NimBLE

使用 `HijelHID_BLEKeyboard` 后，不再走旧 `ESP32 BLE Keyboard` 的 Bluedroid `BLE2902` 回调代码，因此可以避开这个崩溃路径。固件侧只调用“启动、配对状态、按下、释放”，不需要改电机、电池、串口协议逻辑。

NimBLE 另一个好处是资源占用通常比 Bluedroid 更低，更适合 ESP32-C3 这类资源比较紧的小板子。

## 当前实现

固件只应该依赖“键盘能力”，不要依赖旧库内部实现。当前功能只需要：启动、判断配对、按下 HID key、释放 HID key。`HijelHID_BLEKeyboard` 已满足这些需求，所以旧后端已从代码里删除。

当前唯一实现是 `HijelHID_BLEKeyboard + NimBLE-Arduino`。如果后续需要完全控制 HID report、鼠标、手柄或多设备行为，再考虑直接基于 `NimBLE-Arduino` 自写 HID 服务。

## 推荐迁移步骤

1. 在 Arduino IDE 库管理器安装 `HijelHID_BLEKeyboard`，依赖 `NimBLE-Arduino` 会自动安装。
2. 打开 [../firmware/esp32c3_kb_motor/esp32c3_kb_motor.ino](../firmware/esp32c3_kb_motor/esp32c3_kb_motor.ino)。
3. 重新编译并烧录。
4. 删除手机/电脑上旧的蓝牙配对记录，重新搜索并配对 `SuperMini KB`。
5. 验证按键按下/抬起时，串口仍输出 `EVT key k1 DOWN/UP`，主机也收到 Enter 键。

固件启用 `HIDLogLevel::Normal` 后，广播成功时串口会输出 `[HijelHID] Advertising as "SuperMini KB"`。可用 `ble` 查看连接、配对和 bond 状态，使用 `ble restart` 重启广播，或使用 `ble clear` 清除板端 bond 后重新广播。

## 后续演进

如果后续 BLE 键盘功能变复杂，例如需要多键组合、媒体键、鼠标、游戏手柄或自定义 HID 报告，可以考虑把 BLE HID 封装成仓库内的独立模块，直接基于 `NimBLE-Arduino` 写 HID 服务。这样依赖更少、可控性更强，但初期改动会比当前方案大。