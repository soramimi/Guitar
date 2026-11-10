"""Verify that both HTTP transports reject an untrusted local TLS certificate."""
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading

with tempfile.TemporaryDirectory(prefix="guitar-tls-test-") as directory:
    root = Path(directory)
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-keyout", str(root / "key.pem"), "-out", str(root / "cert.pem"),
        "-days", "1", "-subj", "/CN=localhost",
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    class Handler(http.server.BaseHTTPRequestHandler):
        def handle(self):
            try:
                super().handle()
            except (ssl.SSLError, BrokenPipeError, ConnectionResetError):
                pass  # Expected when the client rejects the TLS certificate.

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Length", "0")
            self.end_headers()

        def log_message(self, *args):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(root / "cert.pem", root / "key.pem")
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        subprocess.run([
            sys.argv[1], "--tls-check", f"https://127.0.0.1:{server.server_port}/",
        ], check=True, timeout=20, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
