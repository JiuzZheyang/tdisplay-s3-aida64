# 编译与烧录说明 — T-Display-S3 AIDA64 监控面板

> 目标硬件：LilyGo T-Display-S3（ESP32-S3，ST7789 170×320 竖屏，SPI 总线）
> 运行环境：ESP-IDF v5.4 + LVGL v9（managed_components）
> 用途：连接 AIDA64 RemoteSensor SSE 接口，实时显示 CPU/GPU 占用率/温度/功率

---

## 1. 准备工具链

### 1.1 拉取源码
```bash
# 方式 A：直接从 Seafile 下载源码包
curl -L -o src.zip "https://seafile.jiuzy.cc/seafdav/T-Display-S3-AIDA64/src.zip"
unzip src.zip -d t-display-s3-aida64
cd t-display-s3-aida64

# 方式 B：git 克隆（如已建仓）
git clone <repo-url> t-display-s3-aida64
cd t-display-s3-aida64
```

### 1.2 安装 ESP-IDF v5.4

ESP-IDF v5.4 已在本机 `/opt/esp-idf`，直接 `source` 即可。如需在其它机器构建：

```bash
# Linux / WSL2 / macOS 通用方式
git clone --recursive --branch v5.4 https://github.com/espressif/esp-idf.git ~/esp-idf
cd ~/esp-idf && ./install.sh esp32s3
source ~/esp-idf/export.sh
```

> 备注：本项目使用 `data_endian = LCD_RGB_DATA_ENDIAN_LITTLE` 与 LVGL v9 API，**必须 v5.4+**；
> 旧版 `swap_color_bytes` 字段在 v5.4 已从 `esp_lcd_panel_io_spi_config_t` 移除。

### 1.3 写入 Seafile 凭据（仅本机需要）

```bash
cat > .seafile.conf <<'EOF'
SEAFILE_URL="https://seafile.jiuzy.cc"
SEAFILE_USER="your_user"
SEAFILE_PASS="your_pass"
SEAFILE_LIBRARY="T-Display-S3-AIDA64"
SEAFILE_FILENAME="aida64-merged.bin"
EOF
```

（不要提交到公开仓库；已加进 `.gitignore`。）

---

## 2. 配置 WiFi 与 AIDA64 服务器

编辑 `main/main.c`：

```c
#define WIFI_SSID     "你的WiFi名"
#define WIFI_PASSWORD "你的WiFi密码"
#define AIDA64_SERVER "192.168.1.121"   // AIDA64 所在 PC 的局域网 IP
```

> PC 端 AIDA64 需启用 **LCD → RemoteSensor**（端口默认 7789），并把 6 个 Shared Values 槽位绑到：
> `SIV4=CPU占用率 / SIV5=GPU占用率 / SIV6=CPU温度 / SIV7=GPU温度 / SIV8=CPU功率 / SIV9=GPU功率`
> （SIV10=运行时长，解析但不显示。）

---

## 3. 一键构建 + 合并 + 上传

```bash
cd t-display-s3-aida64
./scripts/build_and_merge.sh          # 构建 + 合并 + 上传
./scripts/build_and_merge.sh --no-upload  # 只构建 + 合并
```

等价手动步骤：
```bash
source /opt/esp-idf/export.sh
idf.py build
idf.py merge-bin -o /workspace/firmware/aida64-merged.bin --format raw --fill-flash-size 4MB
./scripts/sync_source_to_seafile.sh   # 上传固件 + 源码 + 本 BUILD.md
```

---

## 4. 烧录固件

### 4.1 推荐：合并后的整片固件（4MB raw）

```bash
# Linux
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before=default_reset --after=hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 firmware/aida64-merged.bin

# Windows (PowerShell)
python -m esptool --chip esp32s3 -p COM10 -b 460800 `
  --before=default_reset --after=hard_reset `
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB `
  0x0 firmware/aida64-merged.bin
```

> 也可以直接用仓库自带脚本：`./scripts/flash.sh`（自动探测 `/dev/ttyUSB*`、`/dev/ttyACM*`）

### 4.2 可选：手动烧录 bootloader + 分区表 + app

```bash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before=default_reset --after=hard_reset \
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0     build/bootloader/bootloader.bin \
  0x8000  build/partition_table/partition-table.bin \
  0x10000 build/t_display_s3_aida64.bin
```

---

## 5. 监控日志

```bash
# Linux
screen /dev/ttyACM0 115200
# 或
python -m serial.tools.miniterm /dev/ttyACM0 115200

# Windows (PowerShell)
python -m serial.tools.miniterm COM10 115200
```

预期正常日志：
```
I (806) lcd_spi: T-Display-S3 LCD initialized: 170x320 SPI ST7789
I (813) lv_port: LVGL initialized: 170x320 portrait, own task
I (854) ui: UI initialized: 170x320 portrait (light minimal)
...
I (2217) main: Got IP: 192.168.1.22
I (2218) AIDA64: connecting 192.168.1.121:7789 ...
I (2229) AIDA64: connected to AIDA64
I (2239) AIDA64: emit cpu=19/44/56 gpu=19/42/32
I (3328) AIDA64: emit cpu=17/44/54 gpu=19/42/33
```

`emit cpu=X/Y/Z gpu=X/Y/Z` 每秒 1 次即为正常工作。

---

## 6. 常见问题（Troubleshooting）

| 现象 | 可能原因 | 处理 |
|---|---|---|
| **黑屏，仅背光亮** | `esp_lcd_panel_disp_on_off(panel, true)` 未调用，或 `invert_color(false)` ST7789 颜色未反，或 LCD PWR(GPIO15) 未拉 | 已修复（`lcd_spi.c`）：加 PWR/反色/disp_on_off/4 色自检 500ms×4 |
| **画面上下/左右颠倒** | `swap_xy` / `mirror(x,y)` 组合不对 | 改 `lcd_spi.c` 里 `esp_lcd_panel_mirror(panel, x, y)` 4 种组合 |
| **画面水平偏移** | ST7789 RAM 240×320，左右 35 列 padding 未补偿 | `esp_lcd_panel_set_gap(panel, 0, 35)` |
| **首次渲染崩 `LoadProhibited`** | LVGL 写操作无 mutex，与渲染任务竞争 | `ui_init()` 整体用 `lvgl_port_lock(0)` 包裹 |
| **`Connection reset by peer` / `ESP_ERR_HTTP_CONNECT`** | `esp_http_client` 走 esp-tls，被 AIDA64 RST | 已改用 lwIP 纯 BSD socket（`aida64.c`） |
| **`SPI2_HOST` 未声明** | BSP 缺 `esp_lcd_panel_io.h` | 已加 include + `REQUIRES esp_driver_spi` |
| **`app partition too small`** | 默认 `SINGLE_APP` 仅 1MB | `sdkconfig.defaults` 改 `SINGLE_APP_LARGE`（factory 1500K） |
| **AIDA64 一直接不上** | PC 防火墙挡 7789 / 共享值没绑 / SSE 没开 | 关闭 PC 防火墙、AIDA64→LCD 启用 Shared Values |

---

## 7. 项目结构

```
t-display-s3-aida64/
├── CMakeLists.txt              # 顶层 project()
├── sdkconfig.defaults          # 分区表 / 默认配置
├── main/
│   ├── main.c                  # 入口：bsp_lcd_init → lv_port_init → ui_init → wifi_init
│   ├── lv_port.c/h             # LVGL v9 端口（mutex + 渲染任务）
│   ├── ui.c                    # CPU/GPU 弧形仪表 UI
│   ├── aida64.c/h              # 纯 BSD socket SSE 客户端 + SIV 解析
│   └── CMakeLists.txt
├── components/bsp/
│   ├── lcd_spi.c/h             # ST7789 SPI 驱动（PWR/反色/gap/4色自检）
│   └── CMakeLists.txt
├── managed_components/lvgl__lvgl/   # LVGL v9（git submodule 或 idf component）
├── scripts/
│   ├── build_and_merge.sh      # 一键：构建 + 合并 + 上传
│   ├── sync_source_to_seafile.sh  # 上传固件 + src.zip + BUILD.md
│   ├── seafile_upload.sh       # 仅上传合并固件
│   └── flash.sh                # 本地 esptool 烧录
├── docs/
│   ├── BUILD.md                # 本文件
│   └── FIRMWARE_RELEASE.md     # 各次构建的变更记录 / 问题根因
├── firmware/
│   └── aida64-merged.bin       # 构建产物
└── .seafile.conf               # Seafile 凭据（gitignore）
```

---

## 8. 关键实现要点

### 8.1 AIDA64 SSE 客户端用裸 BSD socket（关键）
AIDA64 自带的 mini HTTP 服务器会主动 RST 经过 esp-tls 封装的连接，而 `esp_http_client` 强制走 esp-tls，即使 `keep_alive=false` 也无济于事。绕过方案：直接用 lwIP `socket()/connect()/write()/read()`，不发任何 TLS/握手探测，与 `curl` 行为一致。

详见 `main/aida64.c` 中 `aida64_socket_connect()` / `aida64_socket_send_get()`。

### 8.2 LVGL v9 多线程互斥
LVGL v9 不是线程安全的；`ui_init()` 创建大量对象时必须用 `lvgl_port_lock(0)` 包裹，与渲染任务（核心 1 的 `lvgl_task`）共用同一把互斥锁。`set_monitor_param` / `set_monitor_connect_state` 同样要点。

### 8.3 AIDA64 Shared Values 字段映射与防误匹配
AIDA64 用 `SIV<n>` 暴露 LCD 槽位，串内格式 `data: Page0|{|}SIV4|45{...|}|SIV5|18{|}...`。
为避免 `SIV4` 误匹配 `SIV40`/`SIV41`…，`parse_int_after_key()` 要求 key 紧跟非字母数字边界符（`|`、`:`、`=`、空格、`{`、行末等）才算命中。

### 8.4 颜色字节序
ESP-IDF v5.4 在 `esp_lcd_panel_dev_config_t` 中加 `data_endian = LCD_RGB_DATA_ENDIAN_LITTLE`，等价旧版 `swap_color_bytes=1`，让 ST7789 进入小端模式读出 RAM。LVGL flush 回调里**不要再**手动交换字节，否则会双重交换得到错色。
