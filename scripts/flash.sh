#!/usr/bin/env bash
# 将合并固件 aida64-merged.bin 整片烧录到 T-Display-S3（从 0x0 一次写入）
# 用法:
#   ./scripts/flash.sh /dev/ttyUSB0          # 指定串口
#   ./scripts/flash.sh                       # 自动探测串口(常见 ttyUSB*/ttyACM*)
# 依赖: esptool (pip install esptool) 或 ESP-IDF 自带环境
set -euo pipefail

LOCAL_BIN="${LOCAL_BIN:-/workspace/firmware/aida64-merged.bin}"
PORT="${1:-}"

if [ ! -f "$LOCAL_BIN" ]; then
  echo "错误: 固件不存在: $LOCAL_BIN" >&2
  echo "请先构建并合并: idf.py build && idf.py merge-bin -o $LOCAL_BIN --format raw --fill-flash-size 4MB" >&2
  exit 1
fi

# 未指定串口时自动探测
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    echo "未找到串口，请手动指定: $0 /dev/ttyXXX" >&2
    exit 1
  fi
  echo "自动选择串口: $PORT"
fi

# 优先使用 ESP-IDF 自带 esptool，否则回退到系统 esptool
ESPTOOL="esptool.py"
if [ -x /root/.espressif/python_env/idf5.4_py3.11_env/bin/esptool.py ]; then
  ESPTOOL="/root/.espressif/python_env/idf5.4_py3.11_env/bin/python -m esptool"
fi

echo "==> 烧录 $LOCAL_BIN -> $PORT (chip esp32s3, 0x0)"
$ESPTOOL --chip esp32s3 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 "$LOCAL_BIN"

echo "完成。设备将自动重启进入 AIDA64 监控界面。"
