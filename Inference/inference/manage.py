"""Start, stop, and inspect only this workspace's managed backend."""
import argparse
import json
import os
import subprocess
import sys
import time
from .control import ROOT, STATE, request
from .guard import matching, terminate_tree


def state():
    try:
        data = json.loads(STATE.read_text(encoding='utf-8'))
        if data.get('root') != str(ROOT.parent):
            raise RuntimeError('Backend ownership file belongs to a different workspace')
        return data
    except FileNotFoundError:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['start', 'stop', 'status'])
    args, extra = parser.parse_known_args()
    data = state()
    live = None
    if data:
        try:
            live = request(data)
        except OSError:
            pass
    if args.action == 'status':
        print(json.dumps(live or {'status': 'stopped'}, indent=2))
        return
    if args.action == 'stop':
        if not data:
            print('Backend is already stopped.')
            return
        if live:
            request(data, 'stop')
        deadline = time.monotonic() + 25
        while live and time.monotonic() < deadline and matching(data['pid'], data['created']):
            time.sleep(0.2)
        # Identity includes creation time and workspace/module, never process name alone.
        owner = matching(data['pid'], data['created'])
        if owner:
            command = owner.cmdline()
            if 'inference.run' not in command or owner.cwd() != str(ROOT.parent):
                raise RuntimeError('Refusing to terminate an unrecognized process')
            terminate_tree(owner)
        for name in ('engine', 'guard'):
            process = matching(data[name + '_pid'], data[name + '_created'])
            if process:
                command = ' '.join(process.cmdline())
                if (name == 'engine' and 'llama-server' not in command) or (name == 'guard' and 'inference.guard' not in command):
                    raise RuntimeError('Unrecognized child process')
                terminate_tree(process)
        if any(matching(data[k], data[t]) for k, t in [('pid','created'), ('engine_pid','engine_created'), ('guard_pid','guard_created')]):
            raise RuntimeError('Some owned processes did not exit')
        STATE.unlink(missing_ok=True)
        print('Backend stopped. API, engine and watchdog have exited; chat history is preserved.')
        return
    if live:
        print(f"Backend is already {live['status']} on port {live['api_port']} (context {live['context']}).")
        return
    if data and matching(data['pid'], data['created']):
        raise RuntimeError('Existing supervisor is unresponsive. Run stop first.')
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    process = subprocess.Popen([sys.executable, '-u', '-m', 'inference.run', '--managed', *extra],
             cwd=ROOT.parent, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
             stderr=subprocess.DEVNULL, creationflags=flags, start_new_session=os.name != 'nt')
    deadline = time.monotonic() + 630
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError('Backend could not start; inspect inference/data/backend.log and engine.log')
        data = state()
        if data:
            try:
                live = request(data)
                if live['status'] == 'ready':
                    print(f"Backend ready at http://127.0.0.1:{live['api_port']} | context {live['context']} | KV {live['kv_cache']}")
                    return
            except OSError:
                pass
        time.sleep(0.5)
    raise RuntimeError('Startup timed out. Run stop and inspect backend.log.')


if __name__ == '__main__':
    main()
