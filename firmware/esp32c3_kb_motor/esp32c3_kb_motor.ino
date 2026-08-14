/**
 * ESP32-C3 SuperMini 综合固件
 * ------------------------------------------------------------------
 *  功能:
 *    1. BQ27220 电池计量 (I2C)            -> 上位机查询电量
 *    2. DRV8833 双电机驱动 (PWM 调速)      -> 上位机命令控制
 *    3. BLE 蓝牙键盘 (HID)                -> 按键上报给手机/电脑
 *    4. 上位机串口协议 (行式文本, 参考 xiaozhi-esp32 atom-echos3r)
 *         - 命令: help / ping / status / battery / motor / key
 *         - 主动事件: EVT key k<n> DOWN|UP
 * ------------------------------------------------------------------
 *  依赖库 (Arduino IDE 库管理器):
 *    - kode_bq27220           (已安装, 你现有的 BQ27220 库)
 *    - HijelHID_BLEKeyboard   (BLE HID 键盘, 基于 NimBLE-Arduino)
 * ------------------------------------------------------------------
 *  硬件接线 (建议, 以实际为准):
 *    BQ27220   SDA -> GPIO5   SCL -> GPIO6   (3.3V / GND)
 *    DRV8833   电机A: AIN1 -> GPIO0  AIN2 -> GPIO1
 *              电机B: BIN1 -> GPIO4  BIN2 -> GPIO7
 *              逻辑侧 VCC=3.3V GND, VMOT 按电机电压单独供电
 *    按键 x1   GPIO10 (接 GND, 内部上拉) = 回车
 *    RGB5050   R -> GPIO2  G -> GPIO3  B -> GPIO9 (共阴接 GND)
 *    上位机     硬件串口 UART1: TX -> GPIO20, RX -> GPIO21 (115200 8N1)
 * ------------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <BQ27220.h>
#include <HijelHID_BLEKeyboard.h>

// ============== 配置区 ==============
// --- BQ27220 I2C ---
const int I2C_SDA = 5;
const int I2C_SCL = 6;
const uint8_t BQ_ADDR = 0x55;
const int CHARGE_THRESHOLD_MA = 30;   // 判定充/放电的电流阈值 (mA)

// --- DRV8833 双电机 (2 输入脚/路, analogWrite 0~255) ---
const int AIN1 = 0, AIN2 = 1;         // 电机A
const int BIN1 = 4, BIN2 = 7;         // 电机B

// --- 按键 -> BLE 键盘码 (HID Usage ID), 现在只保留 1 个 = 回车 ---
const int NUM_KEYS = 1;
const uint8_t BUTTON_PINS[NUM_KEYS] = {10};
// HID 键盘 Usage ID: 0x28 = 回车
const uint8_t KEY_HID[NUM_KEYS] = {0x28};
const unsigned long DEBOUNCE_MS = 30;

// --- RGB5050 三色 LED (共阴 -> GND, analogWrite 0~255 调亮度) ---
const int RGB_R = 2, RGB_G = 3, RGB_B = 9;
uint8_t rgbVal[3] = {0, 0, 0};        // 当前 RGB (0-255)

// --- 上位机串口 (硬件 UART1, 不用 USB CDC) ---
const unsigned long BAUD = 115200;
const int UART_TX = 20, UART_RX = 21;
const unsigned long BATTERY_REPORT_MS = 5000;  // 主动上报电量周期
// ==================================

BQ27220 battery;
HijelHID_BLEKeyboard bleKb("SuperMini KB", "ESP32-C3", 100);

// 运行状态
bool keyState[NUM_KEYS] = {false};        // 当前稳定按键状态
bool debState[NUM_KEYS] = {false};        // 去抖中间状态
unsigned long debTime[NUM_KEYS] = {0};
int motorSpeed[2] = {0, 0};               // 当前电机速度 -255..255

char lineBuf[128];
int lineLen = 0;
unsigned long lastBatteryReport = 0;

// ---------- BLE 键盘后端适配 ----------
void bleKeyboardBegin() {
  bleKb.setLogLevel(HIDLogLevel::Off);
  bleKb.begin();
}

bool bleKeyboardReady() {
  return bleKb.isPaired();
}

void bleKeyboardPress(uint8_t key) {
  bleKb.press(key);
}

void bleKeyboardRelease(uint8_t key) {
  bleKb.release(key);
}

// ---------- 电机 ----------
void drivePwm(int p1, int p2, int speed) {   // speed -255..255
  if (speed == 0)        { analogWrite(p1, 0);    analogWrite(p2, 0); }
  else if (speed > 0)    { analogWrite(p1, speed); analogWrite(p2, 0); }
  else                   { analogWrite(p1, 0);    analogWrite(p2, -speed); }
}

void setMotor(int id, int speed) {
  if (speed > 255) speed = 255;
  if (speed < -255) speed = -255;
  if (id == 0) { motorSpeed[0] = speed; drivePwm(AIN1, AIN2, speed); }
  else if (id == 1) { motorSpeed[1] = speed; drivePwm(BIN1, BIN2, speed); }
}

// ---------- RGB5050 三色 LED (共阴) ----------
void setRgb(int r, int g, int b) {
  if (r < 0) r = 0; if (r > 255) r = 255;
  if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255;
  rgbVal[0] = r; rgbVal[1] = g; rgbVal[2] = b;
  analogWrite(RGB_R, r);
  analogWrite(RGB_G, g);
  analogWrite(RGB_B, b);
}

// ---------- 按键: 单一数据源, 同时扇出到 串口 + BLE ----------
void setKeyState(int idx, bool pressed) {
  if (idx < 0 || idx >= NUM_KEYS || keyState[idx] == pressed) return;
  keyState[idx] = pressed;
  // 通路1: 串口上报 (UART1 上位机 + USB 调试都发)
  Serial.printf("EVT key k%d %s\r\n", idx + 1, pressed ? "DOWN" : "UP");
  Serial1.printf("EVT key k%d %s\r\n", idx + 1, pressed ? "DOWN" : "UP");
  // 通路2: BLE 键盘 (未连接时跳过, 不影响串口)
  if (bleKeyboardReady()) {
    if (pressed) bleKeyboardPress(KEY_HID[idx]);
    else         bleKeyboardRelease(KEY_HID[idx]);
  }
}

void pollButtons() {
  for (int i = 0; i < NUM_KEYS; i++) {
    bool raw = !digitalRead(BUTTON_PINS[i]);   // INPUT_PULLUP, 按下=低
    if (raw != debState[i]) {
      debState[i] = raw;
      debTime[i] = millis();
    } else if (raw != keyState[i] && (millis() - debTime[i]) >= DEBOUNCE_MS) {
      setKeyState(i, raw);
    }
  }
}

// ---------- 电量 ----------
void readBattery(int &level, bool &charging, bool &discharging) {
  level = battery.readStateOfChargePercent();      // 0..100, -1 失败
  int ma = battery.readAverageCurrentMilliamps();  // INT16_MIN=-32768 失败
  if (level < 0) level = 0;
  charging = (ma > CHARGE_THRESHOLD_MA);
  discharging = (ma < -CHARGE_THRESHOLD_MA);
  if (ma == -32768) { charging = false; discharging = false; }
}

void reportBattery(Stream &s) {
  int level; bool chg, dchg;
  readBattery(level, chg, dchg);
  s.printf("battery level=%d charging=%d discharging=%d\r\n",
           level, chg ? 1 : 0, dchg ? 1 : 0);
}

void printStatusJson(Stream &s) {
  int level; bool chg, dchg;
  readBattery(level, chg, dchg);
  int mv = battery.readVoltageMillivolts();
  int ma = battery.readAverageCurrentMilliamps();
  if (mv < 0) mv = 0;
  if (ma == -32768) ma = 0;

  s.print("{\"battery\":{\"level\":"); s.print(level);
  s.print(",\"charging\":"); s.print(chg ? 1 : 0);
  s.print(",\"discharging\":"); s.print(dchg ? 1 : 0);
  s.print(",\"mv\":"); s.print(mv);
  s.print(",\"ma\":"); s.print(ma);
  s.print("},\"motor\":{\"m0\":"); s.print(motorSpeed[0]);
  s.print(",\"m1\":"); s.print(motorSpeed[1]);
  s.print("},\"keys\":{\"k1\":"); s.print(keyState[0] ? 1 : 0);
  s.print("},\"rgb\":["); s.print(rgbVal[0]); s.print(',');
  s.print(rgbVal[1]); s.print(','); s.print(rgbVal[2]);
  s.print("],\"ble\":{\"connected\":"); s.print(bleKeyboardReady() ? 1 : 0);
  s.println("}}");
}

void printHelp(Stream &s) {
  s.println("=== ESP32-C3 SuperMini (BQ27220 + DRV8833 + BLE KB) ===");
  s.println("Commands (case-insensitive):");
  s.println("  status                  - full status JSON (battery/motor/keys/rgb/ble)");
  s.println("  battery                 - battery only: level/charging/discharging");
  s.println("  motor <0|1> <fwd|rev|stop> [0-255]  - motor control");
  s.println("  key <1> <0|1>           - simulate button (debug, key1=Enter)");
  s.println("  rgb <r> <g> <b>         - set RGB5050 color (0-255 each)");
  s.println("  ping                    - heartbeat");
  s.println("  help / ?                - this help");
}

// ---------- 串口命令处理 (行式, 参考 atom-echos3r.cc) ----------
void handleCommand(String &line, Stream &out) {
  line.trim();
  String cmd = line;
  String arg;
  int sp = line.indexOf(' ');
  if (sp >= 0) { arg = line.substring(sp + 1); cmd = line.substring(0, sp); }
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    printHelp(out);
  } else if (cmd == "ping") {
    out.println("OK ping");
  } else if (cmd == "status") {
    printStatusJson(out);
  } else if (cmd == "battery") {
    reportBattery(out);
  } else if (cmd == "motor") {
    int id = -1, speed = 255;
    char dir[8] = {0};
    if (sscanf(arg.c_str(), "%d %s %d", &id, dir, &speed) < 2) {
      out.println("ERR motor usage: motor <0|1> <fwd|rev|stop> [0-255]");
      return;
    }
    if (id != 0 && id != 1) { out.println("ERR bad motor id"); return; }
    String d = String(dir); d.toLowerCase();
    if (d == "stop")        setMotor(id, 0);
    else if (d == "fwd")    setMotor(id, speed);
    else if (d == "rev")    setMotor(id, -speed);
    else { out.println("ERR bad motor dir"); return; }
    out.printf("OK motor %d\r\n", id);
  } else if (cmd == "key") {
    int id = 0, st = 0;
    if (sscanf(arg.c_str(), "%d %d", &id, &st) == 2 && id == 1) {
      setKeyState(id - 1, st != 0);
      out.printf("OK key %d\r\n", id);
    } else {
      out.println("ERR key usage: key <1> <0|1>");
    }
  } else if (cmd == "rgb") {
    int r = -1, g = -1, b = -1;
    if (sscanf(arg.c_str(), "%d %d %d", &r, &g, &b) == 3) {
      setRgb(r, g, b);
      out.println("OK rgb");
    } else {
      out.println("ERR rgb usage: rgb <r> <g> <b> (0-255)");
    }
  } else {
    out.println("ERR unknown command (type help)");
  }
}

void processStream(Stream &s) {
  while (s.available()) {
    char c = s.read();
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      String line = String(lineBuf);
      lineLen = 0;
      handleCommand(line, s);
    } else if (c != '\r' && lineLen < (int)sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(BAUD);                                   // USB CDC 仅调试
  Serial1.begin(BAUD, SERIAL_8N1, UART_RX, UART_TX);    // 上位机协议 (UART1)
  delay(200);

  // RGB5050 (共阴)
  pinMode(RGB_R, OUTPUT); pinMode(RGB_G, OUTPUT); pinMode(RGB_B, OUTPUT);
  setRgb(0, 0, 0);                                     // 默认熄灭

  // I2C + BQ27220 (ESP32 需显式指定 SDA/SCL)
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!battery.begin(Wire, BQ_ADDR, I2C_SDA, I2C_SCL, 400000)) {
    Serial.println("WARN BQ27220 init failed, check wiring");
  }

  // DRV8833
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  // N20 减速电机 (3~6V): PWM 提到 15kHz, 减少低转速"吱吱"噪音
  analogWriteFrequency(AIN1, 15000);
  analogWriteFrequency(AIN2, 15000);
  analogWriteFrequency(BIN1, 15000);
  analogWriteFrequency(BIN2, 15000);

  // 按键 (内部上拉, 按下=低)
  for (int i = 0; i < NUM_KEYS; i++) pinMode(BUTTON_PINS[i], INPUT_PULLUP);

  // BLE 键盘 (C3 仅支持 BLE)
  bleKeyboardBegin();

  Serial.println("READY");                 // USB 调试
  Serial1.println("READY");                // 上位机 UART
  printHelp(Serial);
  printHelp(Serial1);
}

void loop() {
  pollButtons();          // 按键 -> 串口 + BLE
  processStream(Serial);  // USB 调试口命令
  processStream(Serial1); // 上位机 UART1 命令

  // 周期主动上报电量
  if (millis() - lastBatteryReport >= BATTERY_REPORT_MS) {
    lastBatteryReport = millis();
    reportBattery(Serial);
    reportBattery(Serial1);
  }
}