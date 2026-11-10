"""Workspace-scoped process ownership and authenticated loopback control."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import hmac
import json
import os
from pathlib import Path
import secrets
import threading
import urllib.request

ROOT = Path(__file__).resolve().parent
STATE = ROOT / 'data' / 'backend.json'


@contextmanager
def ownership():
    STATE.parent.mkdir(parents=True, exist_ok=True)
    with (STATE.parent / 'backend.lock').open('a+b') as file:
        file.seek(0)
        if os.name == 'nt':
            import msvcrt
            if file.read(1) == b'':
                file.write(b'0')
                file.flush()
            file.seek(0)
            try:
                msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError:
                raise RuntimeError('This workspace already has a backend supervisor')
        else:
            import fcntl
            fcntl.flock(file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            yield
        finally:
            if os.name == 'nt':
                file.seek(0)
                msvcrt.locking(file.fileno(), msvcrt.LK_UNLCK, 1)


def request(state, action='status'):
    req = urllib.request.Request(f"http://127.0.0.1:{state['control_port']}/{action}",
        headers={'Authorization': 'Bearer ' + state['token']}, data=b'' if action == 'stop' else None)
    with urllib.request.urlopen(req, timeout=2) as response:
        return json.load(response)


class Control:
    def __init__(self, state, stop):
        self.state = state
        state['token'] = secrets.token_urlsafe(32)
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass
            def handle_request(self):
                if not hmac.compare_digest(self.headers.get('Authorization', ''), 'Bearer ' + state['token']):
                    self.send_error(403)
                    return
                if self.path == '/stop' and self.command == 'POST':
                    stop.set()
                elif self.path != '/status' or self.command != 'GET':
                    self.send_error(404)
                    return
                body = json.dumps({k: v for k, v in state.items() if k != 'token'}).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            do_GET = handle_request
            do_POST = handle_request
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.server.daemon_threads = True
        state['control_port'] = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.save()

    def save(self):
        temporary = STATE.with_suffix('.tmp')
        temporary.write_text(json.dumps(self.state), encoding='utf-8')
        if os.name != 'nt':
            temporary.chmod(0o600)
        temporary.replace(STATE)

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        STATE.unlink(missing_ok=True)
