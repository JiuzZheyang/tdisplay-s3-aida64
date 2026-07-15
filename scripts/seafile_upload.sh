#!/usr/bin/env bash
# 将固件上传到 Seafile（按用户约定：先删除旧固件，再上传新固件）
# 用法: ./scripts/seafile_upload.sh [本地bin路径，默认 /workspace/firmware/aida64-merged.bin]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/../.seafile.conf"

LOCAL_BIN="${1:-/workspace/firmware/aida64-merged.bin}"
REMOTE_DIR="/seafdav/${SEAFILE_LIBRARY}/"
BASE="${SEAFILE_URL}${REMOTE_DIR}"
# 用户名含 '@' 时 curl -u 会误解析，改为显式 Authorization 头
AUTH_HDR="Authorization: Basic $(printf '%s:%s' "${SEAFILE_USER}" "${SEAFILE_PASS}" | base64 -w0)"

if [ ! -f "$LOCAL_BIN" ]; then
  echo "错误: 本地固件不存在: $LOCAL_BIN" >&2
  exit 1
fi

echo "==> 目标: ${BASE}${SEAFILE_FILENAME}"

echo "-- 删除旧固件 --"
curl -sS -H "${AUTH_HDR}" -X DELETE "${BASE}${SEAFILE_FILENAME}" -w "  DELETE -> HTTP %{http_code}\n" || echo "  (忽略删除失败)"

echo "-- 确保目录存在 --"
curl -sS -H "${AUTH_HDR}" -X MKCOL "${BASE}" -w "  MKCOL  -> HTTP %{http_code}\n" || echo "  (忽略建目录失败)"

echo "-- 上传新固件 --"
curl -sS -H "${AUTH_HDR}" -T "$LOCAL_BIN" "${BASE}${SEAFILE_FILENAME}" -w "  PUT    -> HTTP %{http_code}\n"

echo "完成: ${BASE}${SEAFILE_FILENAME}"
