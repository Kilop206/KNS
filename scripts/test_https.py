"""Exercise the actual desktop Hub client against ephemeral local TLS servers."""
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading


def main():
    executable = Path(sys.argv[1]).resolve()
    openssl = sys.argv[2]
    graph = {"nodes": 2, "links": [{"from": 0, "to": 1, "bandwidth": 100, "delay": 1, "loss": 0}]}
    document = {"topology": {"id": "tls-test", "title": "TLS test", "description": "",
                              "visibility": "PUBLIC", "version": 0}, "graph": graph}

    class Handler(http.server.BaseHTTPRequestHandler):
        requests = 0

        def do_GET(self):
            Handler.requests += 1
            body = json.dumps(document).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass

    with tempfile.TemporaryDirectory(prefix="kns-tls-") as temp:
        folder = Path(temp)
        for name in ("server", "untrusted"):
            subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-keyout", str(folder / (name + ".key")), "-out", str(folder / (name + ".pem")),
                            "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost"],
                           check=True, capture_output=True)
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(folder / "server.pem", folder / "server.key")
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            for label, host, trust, success in (
                ("trusted CA and hostname", "localhost", "server", True),
                ("untrusted certificate", "localhost", "untrusted", False),
                ("wrong hostname", "127.0.0.1", "server", False),
            ):
                before = Handler.requests
                env = {**os.environ, "SSL_CERT_FILE": str(folder / (trust + ".pem")),
                       "KNS_TOPOLOGY_HUB_URL": f"https://{host}:{server.server_port}",
                       "KNS_TOPOLOGY_HUB_TOKEN": "", "KNS_AUTO_START": "1"}
                result = subprocess.run([str(executable), "--headless", "--hub-topology", "tls-test",
                                         "--output", str(folder / "metrics.csv")],
                                        cwd=folder, env=env, capture_output=True, text=True, timeout=30)
                if success:
                    assert result.returncode == 0, (label, result.stderr)
                    assert Handler.requests == before + 1, label
                else:
                    assert result.returncode != 0 and "SSL" in result.stderr, (label, result.stderr)
                    assert Handler.requests == before, label
                print("PASS:", label)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    main()
