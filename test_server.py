import http.server
import socketserver
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765

BODY = b"hello world body"

class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="text/plain", headers=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (headers or []):
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self._send(200, BODY)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            self._send(200, BODY)
        elif path == "/chunked":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for part in [b"Hello ", b"chunked ", b"world"]:
                self.wfile.write(b"%x\r\n" % len(part))
                self.wfile.write(part)
                self.wfile.write(b"\r\n")
            self.wfile.write(b"0\r\nX-Trailer: yes\r\n\r\n")
        elif path == "/redir":
            self.send_response(302)
            self.send_header("Location", "/target")
            self.send_header("Content-Length", "0")
            self.end_headers()
        elif path == "/target":
            self._send(200, b"target body reached")
        elif path == "/download/data.txt":
            self._send(200, b"file contents here", headers=[
                ("Content-Disposition", 'attachment; filename="saved.txt"')])
        elif path == "/large":
            data = bytes((i % 251) for i in range(5000))
            rng = self.headers.get("Range")
            if rng:
                start = int(rng.split("=")[1].split("-")[0])
                chunk = data[start:]
                self.send_response(206)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(chunk)))
                self.send_header("Content-Range", "bytes %d-%d/%d" % (start, len(data) - 1, len(data)))
                self.end_headers()
                self.wfile.write(chunk)
            else:
                self._send(200, data, ctype="application/octet-stream")
        else:
            self._send(404, b"not found")

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        data = self.rfile.read(n) if n else b""
        reply = b"received:" + data
        self._send(200, reply)

with socketserver.ThreadingTCPServer(("127.0.0.1", PORT), H) as httpd:
    httpd.serve_forever()
