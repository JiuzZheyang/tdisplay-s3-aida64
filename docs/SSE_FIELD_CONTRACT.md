# SSE 字段契约（服务端 → ESP32）

> 修复"屏幕空 / 信息为空"的关键前提：**PC 端 AIDA64 RemoteSensor（或等价 SSE 服务）必须按下列字段名推送数据**。
> ESP32 端（`main/aida64.c`）已做健壮解析，兼容多种格式；下方为约定的标准格式。

## 1. 端点
- URL：`http://<PC_IP>:7789/sse`
- 方法：`GET`，请求头 `Accept: text/event-stream`
- 周期：建议 500ms–1000ms 推送一次

## 2. 字段清单（6 项）
| 字段名 | 含义 | 单位 | 示例 |
|---|---|---|---|
| `CPU_Rate` | CPU 占用率 | % (0–100) | `45` |
| `CPU_Temp` | CPU 温度 | °C | `62` |
| `CPU_Power` | CPU 封装功率 | W | `65` |
| `GPU_Rate` | GPU 占用率 | % (0–100) | `30` |
| `GPU_Temp` | GPU 温度 | °C | `55` |
| `GPU_Power` | GPU 功率 | W | `40` |

## 3. 支持的推送格式（ESP32 端均兼容）
以下任一格式都能被正确解析（解析器会跳过 `key` 后的非数字字符，兼容 ` ` / `:` / `=` 及 `data:` 前缀、JSON）：

```
# 格式 A：空格分隔，单行（原工程格式）
CPU_Rate 45 CPU_Temp 62 CPU_Power 65 GPU_Rate 30 GPU_Temp 55 GPU_Power 40

# 格式 B：每行一个字段 + 空行事件边界（推荐，标准 SSE）
data: CPU_Rate 45
data: CPU_Temp 62
data: CPU_Power 65
data: GPU_Rate 30
data: GPU_Temp 55
data: GPU_Power 40

# 格式 C：冒号 / 等号
CPU_Rate:45;CPU_Temp:62;GPU_Rate:30;GPU_Temp:55

# 格式 D：JSON
{"CPU_Rate":45,"CPU_Temp":62,"CPU_Power":65,"GPU_Rate":30,"GPU_Temp":55,"GPU_Power":40}
```

## 4. AIDA64 端配置建议
在 AIDA64 → **File → Preferences → Hardware Monitoring → External Applications / LCD**
（或所用 RemoteSensor 工具的 OSD 映射）中，将以下传感器项映射为上述 6 个字段名推送：
- CPU 使用率 → `CPU_Rate`
- CPU 温度（如 `CPU Package` / `CPU Temp`）→ `CPU_Temp`
- CPU 功耗（`CPU IA Power` / `CPU Package Power`）→ `CPU_Power`
- GPU 使用率（`GPU Usage` / `D3D Usage`）→ `GPU_Rate`
- GPU 温度（`GPU Temperature` / `GPU Diode`）→ `GPU_Temp`
- GPU 功耗（`GPU Power` / `Board Power`）→ `GPU_Power`

> 注：具体传感器项名称随 AIDA64 版本与硬件而异，请以本机 AIDA64 传感器列表为准。
> 只要**字段名**与上面一致，ESP32 端无需改动即可正确显示。

## 5. 兼容性
- 旧版仅含 `CPU_Rate/CPU_Temp/MEM_*` 的推送：ESP32 会优雅降级，CPU 正常显示、GPU 显示 0%（不会黑屏/崩溃）。
- 缺失字段以 0 占位；只要出现 `CPU_Rate` 即触发一次 UI 更新。
