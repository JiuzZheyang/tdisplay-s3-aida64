# 固件发布记录 — T-Display-S3 AIDA64

> 编译环境：ESP-IDF v5.4（本机 `/opt/esp-idf`）+ LVGL v9（managed_components）
> 目标芯片：ESP32-S3（T-Display-S3，ST7789 170×320 竖屏）

## 本次构建解决的问题（修复链路）

| 现象 | 根因 | 修复 |
|------|------|------|
| `SPI2_HOST` 未声明 | BSP 缺 `esp_lcd_panel_io.h`，连锁导致 `esp_lcd_spi_bus_handle_t` 未定义 | `lcd_spi.h` 增加 `#include "esp_lcd_panel_io.h"`；BSP `CMakeLists` 增加 `esp_driver_spi`/`esp_driver_gpio` 依赖 |
| `swap_color_bytes` 不是 `esp_lcd_panel_io_spi_config_t` 成员 | v5.4 该字段已从 SPI IO 配置移除 | 改为在 `esp_lcd_panel_dev_config_t` 设置 `.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE`（等价旧版 swap） |
| `aida64.h` 中 `TAG` 宏与 `static const char *TAG` 冲突 | 头文件里冗余 `#define TAG "AIDA64"` | 删除该宏，由各 `.c` 自行声明 |
| `fatal error: esp_ip4_addr.h` | v5.4 已移除该头文件 | `main.c` 删除该 include（`IPSTR`/`IP2STR` 经 `esp_netif.h` 间接可用） |
| `implicit declaration of memcpy` | `main.c` 缺 `<string.h>` | 增加 `#include <string.h>` |
| `main.c: lcd_spi.h: No such file` | main 组件未依赖 bsp | `main/CMakeLists.txt` 增加 REQUIRES `bsp` |
| `app partition is too small`（固件 0x136270 > 1MB） | 默认 `SINGLE_APP` 分区 factory 仅 1MB | 切换到 `PARTITION_TABLE_SINGLE_APP_LARGE`（factory 1500K），余量 17% |
| `E esp-tls: delayed connect error: Connection reset by peer` / `HTTP perform failed: ESP_ERR_HTTP_CONNECT` | AIDA64 自带的 mini HTTP 服务器会主动 RST 经过 `esp-tls` 封装的连接；`esp_http_client` 默认走 esp-tls，即使 `keep_alive=false`、显式 GET、禁重定向也会被 RST（PC 端 `curl` 普通 GET 正常） | `aida64.c` 弃用 `esp_http_client`，改用 lwIP 纯 BSD socket（`socket`/`connect`/`write` GET `/sse`/`read` 循环喂 `sse_feed`），彻底绕开 esp-tls |

> 注：此前已修复「屏幕空白」根因——LVGL 写操作缺 mutex 锁 + SSE 解析脆弱，并据此重构了浅色极简 UI（CPU/GPU 占用率/温度/功率 6 项）。

## 构建产物

- 应用固件：`build/t_display_s3_aida64.bin`（0x136270 ≈ 1.27 MB）
- **合并固件**：`firmware/aida64-merged.bin`（4MB raw，可从 0x0 整片烧录）
  - bootloader @ 0x0 · partition-table @ 0x8000 · app @ 0x10000

## 上传 Seafile（按约定：先删后传）

- 服务端：`https://seafile.jiuzy.cc`
- 资源库：`T-Display-S3-AIDA64`
- 文件路径：`/seafdav/T-Display-S3-AIDA64/aida64-merged.bin`
- 本次结果：
  - `DELETE` 旧固件 → HTTP 404（首传无旧文件，符合预期）
  - `MKCOL` 建目录 → HTTP 405（目录已存在，符合预期）
  - `PUT` 新固件 → **HTTP 201 Created**
- 完整性校验：远端 GET 返回 HTTP 200、4194304 字节，前 16 字节与本地逐位一致 ✅

## 复现命令

```bash
# 1. 编译
bash -c 'source /opt/esp-idf/export.sh; export IDF_PATH=/opt/esp-idf; cd /workspace; /opt/esp-idf/tools/idf.py build'

# 2. 合并为整片固件
bash -c 'source /opt/esp-idf/export.sh; export IDF_PATH=/opt/esp-idf; cd /workspace; /opt/esp-idf/tools/idf.py merge-bin -o /workspace/firmware/aida64-merged.bin --format raw --fill-flash-size 4MB'

# 3. 上传（先删后传）
bash /workspace/scripts/seafile_upload.sh
```

> 凭据与上传规则已固化在 `.seafile.conf`（已 gitignore）与 `scripts/seafile_upload.sh`，每次上传均先 DELETE 再 PUT。

## 已知问题修复：首次渲染崩溃（LoadProhibited / 对象指针 0xffffffff）

**现象**：LCD 初始化、LVGL 任务都正常，但一进首次渲染就 `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)`，崩溃栈在 `lv_obj_get_scrollbar_area` → `lv_obj_scrollbar_invalidate` → `lv_obj_move_to`，对象指针为 `0xffffffff`，屏幕只亮背光、反复重启。

**真实根因（LVGL 多线程竞态，非旋转）**：
- `app_main` 在核心0 调用 `lv_port_init()`（内部已启动核心1 的 LVGL 渲染任务），随后**无锁**地调用 `ui_init()` 创建大量 LVGL 对象；
- 核心1 的渲染任务同时跑 `lv_task_handler()`（已在锁内）遍历对象树；
- LVGL v9 非线程安全，`ui_init` 建对象（尤其是 `lv_ll_ins` 往父对象 children 链表插节点，需多次改 `next/prev` 指针）与渲染遍历并发→渲染读到半初始化的 `next` 指针（`0xffffffff`，LV_MEM 分配出的脏内存）→ 崩溃。

> 旁证：`sdkconfig` 中 `# CONFIG_SPIRAM is not set`（PSRAM 未启用），对象内存在内部 RAM 正常清零，故 `0xffffffff` 来自并发改写堆/对象树，而非未初始化 PSRAM。另外 LVGL v9 的 partial 渲染按缓冲大小分块，旋转并不会造成缓冲越界——所以"去掉 LVGL 层旋转"对崩溃无效（但保留它可避免双重旋转导致朝向错误，仍是正确的）。

**修复（main/ui.c）**：
1. `ui_init()` 整体用 `lvgl_port_lock(0)` / `lvgl_port_unlock()` 包裹，与渲染任务共用同一把互斥锁，序列化所有 LVGL 对象创建。
   （`set_monitor_param` / `set_monitor_connect_state` 此前已正确加锁，`ui_init` 是遗漏点。）
2. 附带：`lv_port.c` 删除 LVGL 层 `lv_display_set_rotation`（旋转仅保留面板层 `swap_xy`）；删除 flush 回调手动 RGB565 字节交换（已由面板层 `data_endian=LITTLE` 完成，避免双重交换）；LVGL 任务栈 8192→10240。

**复测**：`idf.py build` → `merge-bin` → 上传 Seafile（DELETE 204 → PUT 201）。重新下载烧录后应能稳定进入 UI、不再重启。

> 若烧录后整体画面上下/左右颠倒（仅朝向问题，非崩溃），在 `lcd_spi.c` 调整 `esp_lcd_panel_mirror(panel_handle, x, y)` 的 true/false 即可（一行改动）。

## 已知问题修复：AIDA64 SSE 连接被 RST（esp-tls 兼容性问题）

**现象**：WiFi 正常连上、拿到 IP，但 AIDA64 监控任务反复 `E esp-tls: [sock=54] delayed connect error: Connection reset by peer` → `E HTTP_CLIENT: Connection failed` → `W AIDA64: HTTP perform failed: ESP_ERR_HTTP_CONNECT`，重连 2s 一次，永远拿不到数据；日志里**从未**出现 `HTTP_EVENT_ON_CONNECTED`/`recv`，说明 RST 发生在 TCP/TLS 握手阶段。PC 端 `netstat` 显示 `7789 LISTENING` 且 ESP32 曾短暂 `ESTABLISHED`，`curl http://localhost:7789/sse` 也能正常返回 SIV 帧。

**根因**：AIDA64 的 RemoteSensor mini HTTP 服务器对 `esp_http_client`（其底层强制走 `esp-tls` 的 socket 封装，会带 TLS/握手探测特征）会主动 RST；而 `curl`、浏览器那种"裸" TCP + 普通 `GET` 请求被正常接受。这是服务器侧的兼容性差异，不是 ESP32 或网络的问题。

**修复（main/aida64.c）**：
1. 删除 `#include "esp_http_client.h"` 与 `aida64_http_event_handler` 回调。
2. 新增 `aida64_socket_connect()`：用 `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)` + `connect()` 直连 `AIDA64_SERVER:7789`，不经 esp-tls；设置 `SO_RCVTIMEO=2s`（便于周期检查退出标志）与 `SO_KEEPALIVE`。
3. `aida64_socket_send_get()`：发送标准 `GET /sse HTTP/1.1` 请求（含 `Accept: text/event-stream`），分包 `write()` 确保整段发出。
4. `aida64_monitor_task()`：进入 `read()` 循环，把每块数据喂给既有 `sse_feed()`（SIV4/5/6/7/8/9 解析不变）；`read` 返回 0（EOF）或错误即关闭重连；超时/EINTR 仅继续循环。
5. `aida64_monitor_stop()`：置 `aida64_running=false` 并 `close()` 当前 socket 强制唤醒阻塞中的 `read()`，实现优雅退出。
6. `main/CMakeLists.txt`：REQUIRES 中 `esp_http_client` 换为 `lwip`。

**复测**：`idf.py build` ✅ → `merge-bin` ✅（4MB）→ 上传 Seafile（DELETE 204 → PUT 201）。重新烧录后预期日志：

```
I AIDA64: connecting 192.168.1.121:7789 ...
I AIDA64: connected to AIDA64
I AIDA64: emit cpu=53/44/88 gpu=31/51/54
I AIDA64: emit cpu=...
```

> 若仍无 `connected to AIDA64`：先确认 PC 端 AIDA64「硬件监视 → 外部应用程序 / RemoteSensor / LCD」已启用并把 6 个 SIV 槽绑到对应传感器，且 `7789` 端口在防火墙放行（同一局域网）。`AIDA64_SERVER` 当前硬编码为 `192.168.1.121`，如需改 IP 直接改 `main.c` 宏。

## 已知问题修复：SPI 黑屏（仅背光亮，DISPON / Invert / PWR 缺失）

**现象**：数据链路完全正常（日志里 `AIDA64: connected to AIDA64` + 持续 `emit cpu=...`），但 LCD 上无任何图像、只有背光。重启后仍如此。

**根因**（对照参考 I80 实现发现，缺 4 步）：
1. **未发 DISPON（0x29）**：`esp_lcd_panel_init()` 不会自动开显示输出，必须显式 `esp_lcd_panel_disp_on_off(panel, true)`，否则 ST7789 像素寄存器不刷新、只有背光可见。
2. **`invert_color(false)`** ：T-Display-S3 ST7789 模组默认 Inversion Off，必须 `invert_color(true)` 才能显示正确色彩。
3. **PWR_EN(GPIO15) 未拉高**：T-Display-S3 v1.x 用 GPIO15 单独控制 LCD 模组模拟电源；背光走 GPIO38 独立通道，**两条电路分开**。未拉 PWR 时 LCD 像素驱动无电，但背光仍亮。
4. **未设 `set_gap(0, 35)`**：170×320 ST7789 实际 RAM 240×320，左右各有 35 列 padding；不补偿会导致水平偏移甚至全部像素被推到屏外不可见。

> 参考实现用了 I80 8-bit 并行总线（`esp_lcd_new_i80_bus`），本项目是 SPI 总线 —— 总线 API 不同，但上面 4 项"显示控制"逻辑是相通的，**总线层不动、只动面板层 + GPIO**。

**修复（components/bsp/lcd_spi.c）**：
- `lcd_spi.h` 新增 `#define LCD_PWR_IO 15`
- `bsp_lcd_init()` 末尾（`disp_on_off` 之前）拉高 `LCD_PWR_IO`
- 调整初始化顺序：`init → invert_color(true) → swap_xy(true) → mirror(false,false) → set_gap(0, 35) → disp_on_off(true)`
- 末尾新增**板级自检**：整屏刷 4 色（红/绿/蓝/白）各 500ms 便于肉眼快速判断面板是否正常工作（在 LVGL 启动前完成，避免被覆盖）

**复测**：烧录后开屏应看到约 2 秒的"红绿蓝白"快速切换（自检），随后进入 AIDA64 浅色极简 UI（CPU/GPU 弧形仪表）。若 4 色没出现，说明 LCD 物理链路或 PWR 仍有硬件问题；若出现但 UI 不可见，看 LOG 是否仍然正常（说明问题在 LVGL 渲染层）。

## 交付物（Seafile 同步）

每次构建后通过 `./scripts/sync_source_to_seafile.sh` 一键发布到 Seafile 同资料库 `T-Display-S3-AIDA64`：

| 文件 | 内容 | 大小 |
|---|---|---|
| `aida64-merged.bin` | 已合并的 4MB 整片固件（bootloader@0 + partition-table@0x8000 + app@0x10000） | 4.0 MB |
| `src.zip` | 完整源码（不含 build/firmware/.git/lvgl 子模块冗余文件） | ~12 MB |
| `BUILD.md` | 编译/烧录/排错完整说明（`docs/BUILD.md` 同步） | 8 KB |

下载链接：`https://seafile.jiuzy.cc/seafdav/T-Display-S3-AIDA64/{aida64-merged.bin,src.zip,BUILD.md}`

> Seafile WebDAV 凭据含 `@` 字符，curl 默认 `-u` 会误解析（表现为 GET 401），脚本已改用显式 `Authorization: Basic ...` 头；浏览器/客户端走 session 或下载链接时不受影响。MD5 校验：本地 `962fb0b5...` ↔ Seafile `962fb0b5...` ✅ 一致。

常用命令（详见 `docs/BUILD.md`）：
```bash
# 一键构建 + 合并 + 上传
./scripts/build_and_merge.sh

# 只构建 + 合并（不上传）
./scripts/build_and_merge.sh --no-upload

# 仅同步源码 + 文档到 Seafile
./scripts/sync_source_to_seafile.sh
```
