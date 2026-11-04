# LS2K0300 驱动测试说明文档

> 目录：`apps/examples/ls_driver_test`
> 平台：龙芯 LoongArch 2K0300 / OpenVela (NuttX RTOS)
> 入口程序：`ls_driver_test`（源文件 `ls_driver_test.c`）

---

## 一、总览

该目录是一套针对 LOONG-HAT（龙芯 2K0300）开发板的外设驱动测试用例集合，覆盖 GPIO（LED / 按键 / 蜂鸣器）、硬件 PWM（呼吸灯）、硬件 UART（串口收发）、硬件 SPI（ADC 读取、SPI Flash 存储）、I2C 传感器（BH1750 光照）、I2C OLED 显示屏、I2C EEPROM 存储等外设。

### 1.1 运行方式

程序被编译为 NuttX 应用 `ls_driver_test`，在板卡串口终端（nsh）下执行：

```bash
# 单项测试
ls_driver_test <选项>

# 默认（无参数）：依次跑 LED、KEY、OLED、Thermal、PWM、Watchdog、RTC、ADC
ls_driver_test
```

支持的子命令（来自 `ls_driver_test.c` 的 `show_usage`）：

| 选项 | 说明 | 依赖宏 |
|------|------|--------|
| `led` | LED 测试（GPIO72/73，高电平点亮） | `CONFIG_LS2K0300_GPIO` + `CONFIG_LS2K0300_PINCTRL` |
| `oled` | SSD1306 OLED 测试（I2C1，地址 0x3C） | `CONFIG_I2C_DRIVER` |
| `key` | 按键 + LED 测试（GPIO87/86 控 GPIO72/73） | `CONFIG_LS2K0300_GPIO` + `CONFIG_LS2K0300_PINCTRL` |
| `thermal` | 温度传感器测试 | `CONFIG_LS2K0300_THERMAL` |
| `pwm` | PWM2 呼吸灯（GPIO88） | `CONFIG_LS2K0300_PWM` |
| `watchdog` | 看门狗定时器测试 | `CONFIG_LS2K0300_WDT` |
| `rtc` | 实时时钟测试 | `CONFIG_LS2K0300_RTC` |
| `adc` | ADC 测试（MCP3204，SPI2） | `CONFIG_LS2K0300_ADC` |
| `sensor` | BH1750 + OLED 光照显示 | `CONFIG_I2C_DRIVER` |
| `eeprom` | EEPROM + BH1750 光照存储 | `CONFIG_I2C_DRIVER` + GPIO + PINCTRL |
| `buzzer` | 蜂鸣器（GPIO75）+ 按键控制 | GPIO + PINCTRL |
| `spiflash` | SPI Flash + ADC（KEY1 读/存，KEY2 退出） | `CONFIG_LS2K0300_SPIIO` + GPIO + PINCTRL |
| `uart2` | UART2 回显（TX=GPIO44 / RX=GPIO45） | GPIO + PINCTRL |
| `all` | 运行全部（默认） | — |
| `-h / --help` | 帮助 | — |

### 1.2 GPIO 引脚总览

| 引脚 | 功能 | 使用该引脚的测试 |
|------|------|------------------|
| GPIO33 | KEY（头文件 `KEY_GPIO_PIN`，实际测试中未使用） | — |
| GPIO44 | UART2 TX（主功能 func=3） | uart2 |
| GPIO45 | UART2 RX（主功能 func=3） | uart2 |
| GPIO50 | I2C1 SCL（主功能 func=3） | oled / sensor / eeprom / spiflash |
| GPIO51 | I2C1 SDA（主功能 func=3） | oled / sensor / eeprom / spiflash |
| GPIO64 | SPI2 SCK（主功能） | adc / spiflash |
| GPIO65 | SPI2 MISO（主功能） | adc / spiflash |
| GPIO66 | SPI2 MOSI（主功能） | adc / spiflash |
| GPIO67 | MCP3204 ADC 软件片选（GPIO 输出） | adc |
| GPIO72 | 红色 LED（GPIO 输出，高电平点亮） | led / key |
| GPIO73 | 绿色 LED（GPIO 输出，高电平点亮） | led / key |
| GPIO75 | 无源蜂鸣器（GPIO 输出，软件方波） | buzzer |
| GPIO85 | SPI Flash 片选（GPIO 输出） | spiflash |
| GPIO86 | KEY2（GPIO 输入，低电平按下） | key / buzzer / eeprom / spiflash / uart2 |
| GPIO87 | KEY1（GPIO 输入，低电平按下） | key / buzzer / eeprom / spiflash |
| GPIO88 | PWM2 输出（第二功能 func=2，蓝色 LED，高电平点亮） | pwm |

### 1.3 通用机制说明

- **pinctrl 复用配置**：所有引脚在用作 GPIO 或外设功能前，统一通过 `/dev/pinctrl0` 设备进行功能选择。`test_common.c` 中的 `pinctrl_set_gpio_function(pin)` 调用 `ioctl(fd, PINCTRLC_SELECTGPIO, pin)` 把引脚设为普通 GPIO 功能；而需要主功能（如 I2C/UART）或第二功能（如 PWM）时，使用 `ioctl(fd, PINCTRLC_SETFUNCTION, &param)` 并指定 `param.para.function`（3=主功能，2=第二功能）。
- **GPIO 读写**：通过 `/dev/gpioN` 节点访问。`GPIOC_SETPINTYPE` 设置输入/输出方向，`GPIOC_READ` 读取电平，`GPIOC_WRITE` 写入电平。LED 为高电平点亮（active-high），按键为低电平按下（active-low）。
- **I2C**：通过 `/dev/i2c1` 节点，使用 `I2CIOC_TRANSFER` 传输 `struct i2c_transfer_s`（内含 `i2c_msg_s`）完成读写。
- **SPI2（裸寄存器）**：ADC 与 SPI Flash 测试均直接访问 SPI2 控制器寄存器（基址 `0x1610c000`，映射到非缓存段 `0x8000000000000000 | 0x1610c000`），并用 `GENERAL_CFG5`（`0x16000114`）开启 SPI2/3 时钟门控。DR 寄存器必须使用 8 位访问。
- **时延**：使用 NuttX 的 `up_mdelay(ms)` / `up_udelay(us)` / `usleep(us)` / `sleep(s)`。

---

## 二、单项测试详解

### 2.1 LED 测试 — `test_led.c`

**工作原理（调用内容）**
- 经 `pinctrl_set_gpio_function()`（`/dev/pinctrl0` + `PINCTRLC_SELECTGPIO`）将 GPIO72、GPIO73 设为 GPIO 功能。
- `open("/dev/gpio72")`、`open("/dev/gpio73")`，`ioctl(GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN)` 设为输出。
- 循环 3 次：`ioctl(GPIOC_WRITE, 1)` 点亮（高电平 = ON）→ `up_mdelay(500)` → `ioctl(GPIOC_WRITE, 0)` 熄灭（低电平 = OFF）→ `up_mdelay(500)`。
- 结束后保持熄灭，`close()` 各 fd。

**引脚**：GPIO72（红）、GPIO73（绿），均为 GPIO 输出、高电平有效。

**运行方法**
```bash
ls_driver_test led
```

**现象**
- 串口打印 `[LED] Setting up GPIO72 / GPIO73 ...`、`[LED] Pinctrl: ... set to GPIO function`。
- 打印 `[LED] Blinking all 2 LEDs 3 times (high=ON, low=OFF)...`。
- 板卡上红、绿两只 LED 同步闪烁 3 次（每周期亮 500ms、灭 500ms，共约 3 秒）。
- 每次状态切换打印 `[LED] LED ON (GPIOxx=HIGH)` / `[LED] LED OFF (GPIOxx=LOW)`。
- 结束打印 `[LED] Done.`，两灯保持熄灭。

---

### 2.2 按键 + LED 测试 — `test_key.c`

**工作原理（调用内容）**
- 移植自 HarmonyOS `loong_hat 02_KEY`。
- `init_led_gpio()`：pinctrl 设 GPIO 功能 → `open("/dev/gpioN")` → `GPIOC_SETPINTYPE(GPIO_OUTPUT_PIN)` → 写 0 熄灭。
- `init_key_gpio()`：同上，但 `GPIOC_SETPINTYPE(GPIO_INPUT_PIN)`。
- `get_key_value()`：`ioctl(GPIOC_READ, &value)` 读取电平（bool）。
- `is_key_pressed()`：下降沿检测（HIGH→LOW）+ 20ms 软件消抖，返回是否按下。
- `set_led()`：`ioctl(GPIOC_WRITE, on ? 1 : 0)`（高电平点亮）。
- 主循环每 10ms 扫描一次 KEY1/KEY2，持续 30 秒（`KEY_TEST_DURATION_MS=30000`）。KEY1 按下翻转红灯，KEY2 按下翻转绿灯。

**引脚**：LED 红 GPIO72、LED 绿 GPIO73（输出，高电平点亮）；KEY1 GPIO87、KEY2 GPIO86（输入，低电平按下）。

**运行方法**
```bash
ls_driver_test key
```

**现象**
- 打印 `[KEY] KEY1(GPIO87) -> toggle red LED(GPIO72)`、`[KEY] KEY2(GPIO86) -> toggle green LED(GPIO73)`，并提示测试持续 30 秒。
- 打印初始电平 `[KEY] Initial: KEY1=HIGH KEY2=HIGH`、`[KEY] Ready. Press keys to test...`。
- 按 KEY1：红灯状态翻转，打印 `[KEY] KEY1 pressed, red ON/OFF`。
- 按 KEY2：绿灯状态翻转，打印 `[KEY] KEY2 pressed, green ON/OFF`。
- 30 秒后自动退出，熄灭两灯并打印 `[KEY] Test complete. All LEDs off.`。

---

### 2.3 OLED 显示测试 — `test_oled.c`

**工作原理（调用内容）**
- 移植自 HarmonyOS `loong_hat_09_OLED`，驱动 SSD1306（128×64）。
- `oled_pinctrl_setup()`：`/dev/pinctrl0` + `PINCTRLC_SETFUNCTION`，把 GPIO50(SCL)、GPIO51(SDA) 设为主功能（function=3）。
- `open("/dev/i2c1", O_RDWR)`。
- `oled_i2c_scan()`：对 0x03–0x77 逐地址发 `I2C_M_READ` 探测，列出应答设备。
- `oled_init()`：发送 SSD1306 初始化指令序列（0xAE 关显示、0x8D 0x14 开电荷泵、0xAF 开显示 等）。
- `oled_fill_screen(0x00)` 清屏；`oled_show_string()` 用 `oled_fonts.h` 中的 8×16 / 6×8 字模逐列写数据（`oled_write_data` 即向 I2C 写 reg=0x40）。
- 显示文字后 `up_mdelay(3000)` 保持 3 秒，再清屏显示 `OLED Test Done`。

**引脚 / 地址**：I2C1 SCL=GPIO50、SDA=GPIO51；OLED 地址 0x3C；标准速 100kHz。

**运行方法**
```bash
ls_driver_test oled
```

**现象**
- 打印 pinctrl 配置过程、I2C1 打开信息。
- 打印 `[OLED] Scanning I2C1 bus (0x03 - 0x77)...`，并列出发现的设备（应能看到 `Found device at 0x3c`）。
- 打印 `[OLED] SSD1306 initialized`。
- OLED 屏幕上依次显示：
  - `Loong Hat`（8×16）
  - `SSD1306 OLED`（8×16）
  - `I2C1 Test OK`（8×16）
  - `128x64 Ready!`（6×8）
- 3 秒后清屏并显示 `OLED Test Done`，串口打印 `[OLED] Done.`。

---

### 2.4 温度传感器测试 — `test_thermal.c`

**工作原理（调用内容）**
- `open("/dev/thermal0", O_RDONLY)`。
- `read(fd, &temp, sizeof(int32_t))` 读取温度（单位 m℃，即千分之一摄氏度）。
- 打印 `Temperature: %d.%d C`，整数部分 `temp/1000`，小数部分 `temp%1000`。
- `close(fd)`。

**引脚**：无（片上温度传感器）。

**运行方法**
```bash
ls_driver_test thermal
```

**现象**
- 打印 `[Thermal] Testing thermal sensor...`。
- 打印当前温度，如 `[Thermal] Temperature: 45.123 C`。
- 打印 `[Thermal] Done.`。

---

### 2.5 PWM 呼吸灯测试 — `test_pwm.c`

**工作原理（调用内容）**
- 移植自 HarmonyOS `pwm_demo`。
- `pwm_pinctrl_setup()`：`/dev/pinctrl0` + `PINCTRLC_SETFUNCTION`，把 GPIO88 设为第二功能（function=2），即 PWM2 输出。
- `open("/dev/pwm2", O_RDONLY)`。
- `ioctl(PWMIOC_SETCHARACTERISTICS, &info)` 设置频率 1kHz、占空比初值 0；`ioctl(PWMIOC_START)` 启动。
- 呼吸循环 3 次（`BREATH_CYCLES=3`），每次：占空比 0→100%（100 步，每步 30ms）、再 100%→0%；每步通过 `PWMIOC_SETCHARACTERISTICS` 更新 `info.duty`（范围 0–65536）。
- 结束 `PWMIOC_STOP` 停止。

**引脚**：GPIO88（PWM2 第二功能 func=2，蓝色 LED，高电平点亮）；频率 1kHz。

**运行方法**
```bash
ls_driver_test pwm
```

**现象**
- 打印 pinctrl 配置、PWM 打开与启动信息 `[PWM] PWM started, breathing...`。
- 打印每个循环 `[PWM] Cycle 1/3` 等。
- 蓝色 LED 呈现呼吸效果：渐亮约 3 秒 → 渐灭约 3 秒，共 3 个周期（约 18 秒）。
- 结束打印 `[PWM] Breathing complete, PWM stopped.`、`[PWM] Done.`。

---

### 2.6 看门狗测试 — `test_watchdog.c`

**工作原理（调用内容）**
- `open("/dev/watchdog0", O_RDWR)`。
- `ioctl(WDIOC_SETTIMEOUT, 5000)` 设置 5000ms 超时。
- `ioctl(WDIOC_START)` 启动看门狗。
- 每秒 `ioctl(WDIOC_KEEPALIVE, 0)` 喂狗一次，共 3 次（`up_mdelay(1000)` 间隔），打印 `Keepalive 1/2/3`。
- `ioctl(WDIOC_STOP)` 停止看门狗。

**引脚**：无。

**运行方法**
```bash
ls_driver_test watchdog
```

**现象**
- 打印 `[Watchdog] Set timeout to 5000 ms`、`[Watchdog] Watchdog started`。
- 每秒打印一次 `[Watchdog] Keepalive 1/2/3`，期间系统不会复位（证明喂狗有效）。
- 打印 `[Watchdog] Watchdog stopped`、`[Watchdog] Done.`。
- 若停止失败或不喂狗，5 秒后系统会自动复位。

---

### 2.7 RTC 测试 — `test_rtc.c`

**工作原理（调用内容）**
- `open("/dev/rtc0", O_RDWR)`。
- `ioctl(RTC_RD_TIME, &rtctime)` 读取当前 RTC 时间，按 `YYYY-MM-DD HH:MM:SS` 打印。
- 用 `mktime` / `localtime` / `strftime` 格式化打印。
- `ioctl(RTC_SET_TIME, &rtctime)` 把时间设为 `2025-01-01 12:00:00`。
- `up_mdelay(2000)` 等 2 秒后再次 `RTC_RD_TIME` 读取，验证秒数在 2–3 之间，打印 `Time verification PASSED`。

**引脚**：无。

**运行方法**
```bash
ls_driver_test rtc
```

**现象**
- 打印当前 RTC 时间 `[RTC] Current RTC time: ...`。
- 打印 `[RTC] Setting time to 2025-01-01 12:00:00...`、`[RTC] Time set successfully`。
- 2 秒后打印 `[RTC] RTC time after 2s: 2025-01-01 12:00:02`（或 :03）。
- 打印 `[RTC] Time verification PASSED`、`[RTC] Done.`。

---

### 2.8 ADC 测试 — `test_adc.c`

**工作原理（调用内容）**
- 驱动 MCP3204（12 位 SPI ADC），使用硬件 SPI2 控制器（裸寄存器方式，不经过 NuttX SPI 驱动框架）。
- `mcp3204_init()`：
  - `pinctrl_set_gpio_function(67)` + `open("/dev/gpio67")` + `GPIOC_SETPINTYPE(GPIO_OUTPUT_PIN)` 把 GPIO67 配为软件片选，并拉高。
  - `CFG5_REG |= (0x3<<17)` 开启 SPI2/3 时钟门控。
  - 配置 SPI2 寄存器：CFG3=主模式+全双工+软件 CS、CFG1=8 位帧 mode0、CFG2=波特分频（APB 200MHz / 1MHz = 200）、CR1 置 SPE 使能。
- `spi2_xfer_byte()`：写 DR（8 位访问）→ 置 CSTART 启动传输 → 等待 EOT → 等 RXA → 读 DR。
- `mcp3204_read_raw(channel)`：拉低 CS → 发 0x06 + (channel<<6) + 0x00 三字节，取中间两字节拼出 12 位原始值；过滤 0xff（MISO 卡高）。
- `mcp3204_read_average()`：最多采样 8 次，取平均。
- `mcp3204_raw_to_mv()`：`raw*3300/4095` 换算电压（Vref=3.3V）。
- 主循环采样 30 轮（`ADC_SAMPLE_COUNT=30`），每轮 500ms，打印原始值与电压。

**引脚 / 参数**：SPI2 SCK=GPIO64、MISO=GPIO65、MOSI=GPIO66（主功能）；CS=GPIO67（软件 GPIO）；CH0 通道；Vref=3300mV；SPI 时钟 1MHz；APB 200MHz。

**运行方法**
```bash
ls_driver_test adc
```

**现象**
- 打印 `[ADC] MCP3204 hardware SPI2 init`、`[ADC] SPI2 init done, Vref=3300 mV`。
- 打印 30 行采样结果，如：
  ```
  [ADC]   1: CH0 raw= 512 voltage=0.413 V
  [ADC]   2: CH0 raw= 514 voltage=0.415 V
  ...
  ```
  （CH0 接电位器时，旋转电位器数值随之变化）
- 打印 `[ADC] Done.`。

---

### 2.9 BH1750 + OLED 光照显示测试 — `test_sensor_oled.c`

**工作原理（调用内容）**
- `open("/dev/i2c1", O_RDWR)`。
- BH1750 初始化 `bh1750_init()`：先探测地址 0x23，失败再试 0x5C；发 `Power On(0x01)` → `Reset(0x07)` → `Continuous H-Resolution(0x10)`，每步间用 `usleep` 等待。
- OLED 初始化 `oled_init_device()`：发送 SSD1306 初始化指令序列。
- 显示初始画面 `Loong Hat` / `Sensor Ready`，`usleep(1s)`。
- 主循环 30 次（`loop_count<30`），每 2 秒：
  - `bh1750_read_lux()`：I2C 读 2 字节，`raw = (buf[0]<<8)|buf[1]`，`lux_x100 = (raw*1000+6)/12`。
  - 打印 `[BH1750] light = xx.xx lux`。
  - `oled_show_light_data()`：清屏后显示 `Loong Hat` + `Lux: xx.xx`。
- 结束清屏显示 `Test Done!`。

**引脚 / 地址**：I2C1 SCL=GPIO50、SDA=GPIO51；BH1750 地址 0x23（或 0x5C）；OLED 地址 0x3C。

**运行方法**
```bash
ls_driver_test sensor
```

**现象**
- 打印 BH1750 探测与初始化结果 `[BH1750] detected addr=0x23`、`[BH1750] init OK`、`[OLED] init OK`。
- OLED 显示 `Loong Hat` / `Sensor Ready` 1 秒。
- 之后每 2 秒刷新一次：OLED 上显示 `Lux: xx.xx`，串口打印 `[BH1750] light = xx.xx lux` 和 `[SENSOR] --- loop N done ---`。
- 遮挡/照射光照传感器，lux 数值相应下降/上升。
- 30 次后清屏显示 `Test Done!`，打印 `[SENSOR] Test complete (30 iterations)`。

---

### 2.10 EEPROM + BH1750 测试 — `test_eeprom.c`

**工作原理（调用内容）**
- AT24C02 EEPROM（256 字节，页大小 8，地址 0x50）。
- `open("/dev/i2c1")`；EEPROM 读写：先写 2 字节内存地址，再读/写数据（`eeprom_read_byte` / `eeprom_write_byte` / `eeprom_write_page` / `eeprom_read_bytes`），写后 `usleep(20ms)` 等待写周期。
- 存储布局：`0x0000`=magic(0xA5)、`0x0001`=读数个数、`0x0010..`=5 个 uint16_t 光照值（lux×100，大端）。
- 启动时 `eeprom_print_contents()` 读取并打印当前 EEPROM 内容。
- `init_key_gpio()` 把 KEY1(87)、KEY2(86) 配为输入。
- 主循环 10ms 扫描：KEY1 按下 → `read_and_save_light()`：初始化 BH1750，读 5 次（间隔 300ms），打印每次读数，再 `eeprom_save_readings()` 写入 EEPROM（先写 count，再分页写数据，最后写 magic），并回读校验、打印更新后的内容。KEY2 按下 → 退出。

**引脚 / 地址**：I2C1 SCL=GPIO50、SDA=GPIO51；EEPROM 0x50；BH1750 0x23/0x5C；KEY1=GPIO87、KEY2=GPIO86。

**运行方法**
```bash
ls_driver_test eeprom
```

**现象**
- 打印 `[EEPROM] Opening /dev/i2c1...`、`[EEPROM] I2C1 opened`。
- 打印当前 EEPROM 内容：若首次运行 magic 非 0xA5，显示 `No valid data stored`；若已有数据则列出每次光照读数。
- 提示 `KEY1(GPIO87) ready, waiting for press...`、`KEY2(GPIO86) ready, press to exit`。
- 按 KEY1：打印 `=== KEY1 pressed: reading light sensor 5 times ===`，5 次 `[EEPROM] [i] light = xx.xx lux`，再 `Saving readings to EEPROM...`、`Saved and verified 5 readings`，并打印更新后的 EEPROM 内容。
- 按 KEY2：打印 `[EEPROM] KEY2 pressed, exiting...` 后退出，打印 `[EEPROM] Test complete.`。

---

### 2.11 蜂鸣器测试 — `test_buzzer.c`

**工作原理（调用内容）**
- 移植自 HarmonyOS `loong_hat 03_buzzer`。无源蜂鸣器由 GPIO 软件翻转产生方波驱动。
- `init_buzzer_gpio(75)`：pinctrl + `open("/dev/gpio75")` + 输出，初始写 0。
- `init_key_gpio()`：KEY1(87)、KEY2(86) 配为输入。
- `buzzer_beep(fd, duration_ms)`：半周期 500us（即 1kHz），按 `duration_ms*1000/(500*2)` 计算循环次数，交替 `GPIOC_WRITE(1/0)` + `up_udelay(500)`；结束写 0。
- `is_key_pressed()`：下降沿 + 20ms 消抖。
- 启动时先响一声 100ms 确认蜂鸣器工作。
- 主循环 10ms 扫描：KEY1 按下 → 响 100ms；每 2000ms 自动响一次；KEY2 按下 → 退出。

**引脚 / 参数**：蜂鸣器 GPIO75（输出）；KEY1=GPIO87、KEY2=GPIO86（输入，低电平按下）；1kHz 方波；beep 时长 100ms；自动响间隔 2000ms。

**运行方法**
```bash
ls_driver_test buzzer
```

**现象**
- 打印 `[BUZZER] Buzzer on GPIO75, 1000Hz tone`、`KEY1(GPIO87): manual beep`、`KEY2(GPIO86): exit`、`Auto-beep every 2000ms`。
- 启动时蜂鸣器短响一声，打印 `[BUZZER] Initial beep...`。
- 按 KEY1：蜂鸣器响 100ms，打印 `[BUZZER] KEY1 pressed, beep!`。
- 每 2 秒无人操作时自动响一次，打印 `[BUZZER] auto beep`。
- 按 KEY2：打印 `[BUZZER] KEY2 pressed, exiting...`，蜂鸣器关闭，打印 `[BUZZER] Test complete.`。

---

### 2.12 SPI Flash + BH1750 测试 — `test_spiflash.c`

**工作原理（调用内容）**
- SPI Flash（如 W25Q/GD25Q）+ BH1750，使用硬件 SPI2（裸寄存器，同 ADC 测试）+ I2C1。
- GPIO：Flash CS=GPIO85（软件片选，输出）、KEY1=87、KEY2=86（输入）。
- `spi2_init()`：开启 CFG5 时钟门控，配置 SPI2 主模式/8 位/mode0/1MHz，CS 拉高。
- `spi2_bus_reset()` / `flash_software_reset()`：复位 SPI 总线与 Flash 芯片（`Enable Reset(0x66)` + `Reset(0x99)`，等 30ms）。
- `flash_read_jedec_id()`：发 `0x9F` 读 JEDEC ID（3 字节），并按 `id[2]` 计算容量 `1<<id[2]`。
- 数据存储在 Flash 最后一个 4KB 扇区 + 0x100 偏移处（`SPIFLASH_TEST_OFFSET=0x100`）。布局同 EEPROM：magic(0xA5)/count/5 个 uint16_t 光照值。
- Flash 操作：`flash_write_enable(0x06)` → `flash_sector_erase(0x20, 4K)` → `flash_page_program(0x02, addr, data)` → `flash_read_data(0x03, addr, buf)`；每次写后 `flash_wait_ready()` 轮询状态寄存器 WIP 位（最多 5 秒）。
- 启动时打印 Flash ID、容量、状态、当前存储内容。
- 主循环 10ms 扫描：KEY1 按下 → `read_light_and_save()`：初始化 BH1750，读 5 次（间隔 300ms），打印每次读数，再 `spiflash_save_readings()` 擦除扇区并写入，回读校验，打印更新后内容；KEY2 按下 → 退出。

**引脚 / 地址**：SPI2 SCK=GPIO64、MISO=GPIO65、MOSI=GPIO66；Flash CS=GPIO85；I2C1 SCL=GPIO50、SDA=GPIO51；BH1750 0x23/0x5C；KEY1=GPIO87、KEY2=GPIO86。SPI 时钟 1MHz，mode0。

**运行方法**
```bash
ls_driver_test spiflash
```

**现象**
- 打印 GPIO 初始化、I2C1/SPI2 初始化信息，`[SPIFLASH] SPI2 init OK (1MHz, mode 0)`。
- 打印 Flash JEDEC ID（如 `EF 40 16` 对应 W25Q32）、容量、状态、扇区/数据地址。
- 打印当前 Flash 内存储的数据（首次为 `invalid/empty`）。
- 提示 `KEY1(GPIO87) ready, waiting...`、`KEY2(GPIO86) press to exit`。
- 按 KEY1：打印 `=== KEY1 pressed: reading BH1750 x5 ===`，5 次 `[SPIFLASH] [i] xx.xx lux`，再 `Erasing sector ...`、`Saved and verified 5 readings`，并打印更新后的 Flash 内容。
- 按 KEY2：打印 `[SPIFLASH] KEY2 pressed, exiting...`，`[SPIFLASH] Test complete.`。

---

### 2.13 UART2 收发测试 — `test_uart2.c`

**工作原理（调用内容）**
- 使用内核串口驱动节点 `/dev/ttyS2`（对应 UART2 控制器，基址 `0x16100800`，16550 兼容，8N1@115200）。
- `uart2_pinctrl_setup()`：`/dev/pinctrl0` + `PINCTRLC_SETFUNCTION`，把 GPIO44(TX)、GPIO45(RX) 再次设为主功能（function=3）。注释说明：即便 bringup 已配置，其他驱动可能在启动过程中覆盖这两脚的复用，故应用层再确认一次。
- `key2_open()`：把 KEY2(86) 配为输入，用于退出回显循环。
- `open("/dev/ttyS2", O_RDWR | O_NONBLOCK)`：非阻塞打开，便于 `read` 返回 `EAGAIN` 时轮询按键。
- `tcgetattr` / `tcsetattr`：配置 raw 模式、8 数据位（CS8）、无校验（PARENB=0）、1 停止位（CSTOPB=0）、`CLOCAL|CREAD`、`cfsetspeed(B115200)`。
- 发送 banner：逐字节 `write` + `tcdrain`。
- 回显循环（最多 64 字节）：`read` 一字节 → 打印 `[UART2] RX: 0x.. ('c')` → `write` 回显；收到 `\r` 或 `\n` 时补发 `\n` 后退出；轮询 KEY2 按下则提前退出。

**引脚 / 参数**：UART2 TX=GPIO44、RX=GPIO45（主功能 func=3）；115200 8N1；KEY2=GPIO86（退出）。

**运行方法**
```bash
ls_driver_test uart2
```

**现象**
- 打印 pinctrl 配置 `[UART2] Pinmux OK (GPIO44/GPIO45 -> func 3)`、`/dev/ttyS2 opened`、`termios configured (8N1 @ 115200)`。
- 打印 `Sending banner (N bytes)...`，此时若用 USB-TTL 连接 GPIO44(TX)/GPIO45(RX)，上位机串口工具会收到：
  ```
  === LS2K0300 UART2 Echo Test (GPIO44 TX / GPIO45 RX) ===
  Type characters, they will be echoed back.
  ```
- 进入回显循环，上位机发送任意字符，串口终端打印 `[UART2] RX: 0x.. ('c')`，同时上位机收到回显字符。
- 收到回车/换行或收到 64 字节后退出；也可按 KEY2 提前退出。
- 退出时打印 `[UART2] Done. Received N bytes.`。

---

## 三、默认（无参数）运行顺序

不带参数执行 `ls_driver_test` 时，按以下顺序依次执行（受宏开关控制）：

1. `test_led()` → `test_key()` （需 GPIO + PINCTRL）
2. `test_oled()` （需 I2C）
3. `test_thermal()` （需 THERMAL）
4. `test_pwm()` （需 PWM）
5. `test_watchdog()` （需 WDT）
6. `test_rtc()` （需 RTC）
7. `test_adc()` （需 ADC）

> 注意：`sensor`、`eeprom`、`buzzer`、`spiflash`、`uart2` 五项需要交互（按键/外接设备），**不在默认序列中**，必须显式传子命令才会运行。

---

## 四、文件清单

| 文件 | 作用 |
|------|------|
| `ls_driver_test.h` | 公共头：引脚宏定义、各测试函数原型 |
| `ls_driver_test.c` | main 入口、子命令分发、usage |
| `test_common.c` | 公共 pinctrl 复用辅助函数 |
| `test_led.c` | LED 闪烁测试 |
| `test_key.c` | 按键 + LED 测试 |
| `test_oled.c` | SSD1306 OLED 显示测试 |
| `test_thermal.c` | 温度传感器测试 |
| `test_pwm.c` | PWM 呼吸灯测试 |
| `test_watchdog.c` | 看门狗测试 |
| `test_rtc.c` | RTC 测试 |
| `test_adc.c` | MCP3204 ADC（SPI2 裸寄存器）测试 |
| `test_sensor_oled.c` | BH1750 + OLED 光照显示测试 |
| `test_eeprom.c` | AT24C02 EEPROM + BH1750 测试 |
| `test_buzzer.c` | 蜂鸣器 + 按键测试 |
| `test_spiflash.c` | SPI Flash + BH1750 测试 |
| `test_uart2.c` | UART2 回显测试 |
| `oled_fonts.h` | SSD1306 字模表（6×8 与 8×16） |
