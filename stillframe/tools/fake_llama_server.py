#!/usr/bin/env python3
"""A tiny stand-in for llama.cpp's llama-server, for engine tests.

Implements just enough of the API the engine uses: GET /props, POST /apply-template, /tokenize,
/completion (SSE streaming). Tests drive it through two extra endpoints:
  POST /_script   {"responses": ["<text the model should produce>", ...]}   queue of canned outputs
  GET  /_log      what the server received (prompts, flags, whether a client hung up mid-stream)
Prints "PORT <n>" on the first line of stdout once listening.
"""
import json, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

STATE = {"responses": [], "log": [], "delay": 0.0, "n_ctx": 4096, "slots": 1}
LOCK = threading.Lock()


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def _json(self, code, obj):
        data = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)

    def _body(self):
        n = int(self.headers.get("Content-Length", 0))
        return json.loads(self.rfile.read(n) or b"{}")

    def do_GET(self):
        if self.path == "/props":
            return self._json(200, {"default_generation_settings": {"n_ctx": STATE["n_ctx"]}, "total_slots": STATE["slots"]})
        if self.path == "/_log":
            with LOCK:
                return self._json(200, STATE["log"])
        self._json(404, {"error": "not found"})

    def do_POST(self):
        body = self._body()
        if self.path == "/_script":
            with LOCK:
                STATE["responses"] = list(body.get("responses", []))
                STATE["delay"] = float(body.get("delay", 0.0))
                STATE["log"] = []
                STATE["n_ctx"] = int(body.get("n_ctx", 4096))
            return self._json(200, {"ok": True})
        if self.path == "/apply-template":
            text = "".join("<|%s|>\n%s\n" % (m["role"], m["content"]) for m in body["messages"]) + "<|assistant|>\n"
            return self._json(200, {"prompt": text})
        if self.path == "/tokenize":
            return self._json(200, {"tokens": [ord(c) for c in body["content"]]})
        if self.path == "/completion":
            prompt = body["prompt"]
            text = "".join(chr(t) for t in prompt) if isinstance(prompt, list) else prompt
            with LOCK:
                out = STATE["responses"].pop(0) if STATE["responses"] else '{"reply":"(no scripted response)"}'
                entry = {"prompt": text, "cache_prompt": body.get("cache_prompt"), "n_predict": body.get("n_predict"),
                         "seed": body.get("seed"), "json_schema": body.get("json_schema"), "id_slot": body.get("id_slot"),
                         "temperature": body.get("temperature"), "aborted": False, "completed": False}
                STATE["log"].append(entry)
            if out == "__ERROR__":
                return self._json(500, {"error": {"message": "boom"}})
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Connection", "close")
            self.end_headers()

            def chunk(s):
                b = s.encode()
                self.wfile.write(b"%x\r\n" % len(b) + b + b"\r\n")
                self.wfile.flush()
            try:
                step = 4
                for i in range(0, len(out), step):
                    time.sleep(STATE["delay"])
                    chunk("data: " + json.dumps({"content": out[i:i + step], "stop": False}) + "\n\n")
                chunk("data: " + json.dumps({"content": "", "stop": True, "stop_type": "eos", "truncated": False, "timings": {"predicted_n": len(out) // 4}}) + "\n\n")
                self.wfile.write(b"0\r\n\r\n")
                with LOCK:
                    entry["completed"] = True
            except (BrokenPipeError, ConnectionResetError):
                with LOCK:
                    entry["aborted"] = True
            return
        self._json(404, {"error": "not found"})


if __name__ == "__main__":
    srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
    print("PORT %d" % srv.server_address[1], flush=True)
    srv.serve_forever()
