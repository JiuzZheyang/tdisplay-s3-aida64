#!/usr/bin/env python3
"""
本地 SSE 模拟服务 —— 用于验证 T-Display-S3 的"数据绑定"（修复①②）。
无需真实 AIDA64：本服务按 SSE 标准格式，每 1 秒推送一次 6 个字段。

用法：
    python3 sse_mock.py [port]      # 默认 7789
然后在 main.c 的 AIDA64_SERVER 指向本机 IP，烧录上板即可看到双弧实时转动。

ESP32 端请求： GET http://<本机IP>:<port>/sse   (Accept: text/event-stream)
"""
import sys
import time
import math
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 7789


def sample_values(t):
    """模拟一组合理的 PC 传感器读数（正弦 + 抖动），仅用于演示。"""
    cpu_rate = int(50 + 40 * (0.5 + 0.5 * math.sin(t / 3.0)))
    cpu_temp = int(55 + 15 * (0.5 + 0.5 * math.sin(t / 5.0)))
    cpu_power = int(45 + 35 * (0.5 + 0.5 * math.sin(t / 3.0)))
    gpu_rate = int(30 + 50 * (0.5 + 0.5 * math.sin(t / 4.0 + 1)))
    gpu_temp = int(45 + 20 * (0.5 + 0.5 * math.sin(t / 6.0 + 1)))
    gpu_power = int(30 + 60 * (0.5 + 0.5 * math.sin(t / 4.0 + 1)))
    return cpu_rate, cpu_temp, cpu_power, gpu_rate, gpu_temp, gpu_power


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") not in ("/sse", ""):
            self.send_response(404)
            self.end_headers()
            return

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        print(f"[mock] SSE 客户端已连接：{self.client_address}")

        t = 0.0
        try:
            while True:
                cpu_rate, cpu_temp, cpu_power, gpu_rate, gpu_temp, gpu_power = sample_values(t)
                # 标准 SSE：每行 "data: key value"，事件间以空行分隔
                payload = (
                    f"data: CPU_Rate {cpu_rate}\n"
                    f"data: CPU_Temp {cpu_temp}\n"
                    f"data: CPU_Power {cpu_power}\n"
                    f"data: GPU_Rate {gpu_rate}\n"
                    f"data: GPU_Temp {gpu_temp}\n"
                    f"data: GPU_Power {gpu_power}\n\n"
                )
                self.wfile.write(payload.encode("utf-8"))
                self.wfile.flush()
                print(f"[mock] -> cpu={cpu_rate}/{cpu_temp}/{cpu_power} "
                      f"gpu={gpu_rate}/{gpu_temp}/{gpu_power}")
                t += 1.0
                time.sleep(1.0)
        except (BrokenPipeError, ConnectionResetError):
            print(f"[mock] 客户端断开：{self.client_address}")

    def log_message(self, fmt, *args):
        pass  # 静默默认访问日志


if __name__ == "__main__":
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"SSE mock server 运行中： http://0.0.0.0:{PORT}/sse  (Ctrl+C 停止)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[mock] 已停止")
        server.shutdown()
