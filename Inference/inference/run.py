"""Start the C++ inference engine and the memory API together."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
import urllib.request
import sys
import threading
import logging
from logging.handlers import RotatingFileHandler
import psutil
from .control import Control, ownership
from .guard import terminate_tree
from .hardware import metadata, choose_context

ROOT = Path(__file__).resolve().parent


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=ROOT.parent / "models" / "model.gguf")
    parser.add_argument("--engine", default=str(ROOT / "bin" / ("llama-server.exe" if os.name == "nt" else "llama-server")))
    parser.add_argument("--gpu-layers", default="auto", help="auto, all, or an explicit positive layer count")
    parser.add_argument("--context", default="auto", help="auto or a token count; auto chooses 4K–32K from estimated VRAM headroom")
    parser.add_argument("--parallel", type=int, default=1)
    parser.add_argument("--device", help="CUDA device identifier, e.g. CUDA0; defaults to first visible NVIDIA GPU")
    parser.add_argument("--cpu", action="store_true", help="CPU-only mode: skip CUDA discovery and force zero GPU layers")
    parser.add_argument("--vram-reserve", type=int, default=1024, help="MiB left free by automatic model fitting")
    parser.add_argument("--kv-cache", choices=["auto", "f16", "q8_0"], default="auto", help="auto uses Q8 KV for the installed Magnum model, FP16 for others")
    parser.add_argument("--template", choices=["auto", "original"], default="auto")
    parser.add_argument("--managed", action="store_true", help="Log to bounded files instead of the console")
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--ubatch-size", type=int, default=128)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--engine-port", type=int, default=8081)
    parser.add_argument("--startup-timeout", type=int, default=600)
    args = parser.parse_args(argv)
    if args.context != 'auto':
        try:
            args.context = int(args.context)
        except ValueError:
            parser.error('--context must be auto or an integer')
        if args.context < 2048:
            parser.error('--context must be at least 2048')
    if args.parallel < 1:
        parser.error('--parallel must be positive')
    if args.gpu_layers not in ("auto", "all") and (not args.gpu_layers.isdecimal() or int(args.gpu_layers) < 1):
        parser.error("--gpu-layers must be auto, all or a positive count")
    if args.vram_reserve < 256 or not 1 <= args.ubatch_size <= args.batch_size:
        parser.error("Reserve at least 256 MiB and use 1 <= ubatch-size <= batch-size")
    if args.startup_timeout < 1 or not 1 <= args.port <= 65535 or not 1 <= args.engine_port <= 65535 or args.port == args.engine_port:
        parser.error("Use positive startup timeout and distinct ports in 1..65535")
    return parser, args


def select_device(output, requested=None):
    devices = re.findall(r"^\s*(CUDA\d+):", output, re.MULTILINE)
    if not devices:
        raise RuntimeError("No CUDA GPU is visible to llama-server. Install an NVIDIA-compatible CUDA build and driver. See inference/README.md")
    if requested is not None and requested not in devices:
        raise RuntimeError(f"Requested device {requested} not available; visible devices: {', '.join(devices)}")
    return requested or devices[0]


def engine_command(args, device):
    context = args.context if isinstance(args.context, int) else 4096
    if args.cpu:
        return [args.engine, "--model", str(args.model.resolve()), "--host", "127.0.0.1",
                "--port", str(args.engine_port), "--ctx-size", str(context * args.parallel),
                "--parallel", str(args.parallel), "--device", "none", "--n-gpu-layers", "0",
                "--no-kv-offload", "--no-op-offload",
                "--fit", "off", "--flash-attn", "off", "--cache-type-k", "f16",
                "--cache-type-v", "f16", "--batch-size", str(args.batch_size),
                "--ubatch-size", str(args.ubatch_size), "--cache-ram", "0",
                "--no-cache-idle-slots", "--cont-batching", "--no-context-shift", "--jinja"]
    command = [args.engine, "--model", str(args.model.resolve()), "--host", "127.0.0.1",
               "--port", str(args.engine_port), "--ctx-size", str(context * args.parallel),
               "--parallel", str(args.parallel), "--device", device, "--split-mode", "none",
               "--fit", "on" if args.gpu_layers == "auto" else "off",
               "--fit-target", str(args.vram_reserve),
               "--flash-attn", "on" if args.kv_cache == "q8_0" else "auto",
               "--cache-type-k", args.kv_cache, "--cache-type-v", args.kv_cache,
               "--batch-size", str(args.batch_size), "--ubatch-size", str(args.ubatch_size),
               "--cache-ram", "0", "--no-cache-idle-slots", "--cont-batching", "--no-context-shift", "--jinja"]
    if getattr(args, 'stable_template', False):
        command.extend(['--chat-template-file', str(ROOT / 'templates' / 'magnum-stable.jinja')])
    # Leaving GPU layer count unset lets the engine fit it to actual free VRAM.
    if args.gpu_layers != "auto":
        command.extend(["--n-gpu-layers", args.gpu_layers])
    return command


def serve(parser, args):
    if not args.model.is_file():
        parser.error(f"Model missing: {args.model}. See models/README.md")
    if not Path(args.engine).is_file() and not shutil.which(args.engine):
        parser.error("llama-server missing. See inference/README.md")
    if args.host not in ("localhost", "127.0.0.1", "::1") and not os.getenv("INFERENCE_API_KEY"):
        parser.error("Set INFERENCE_API_KEY before exposing the API on the network")
    if args.cpu:
        discovered = subprocess.CompletedProcess([], 0, stdout="", stderr="")
        device = "none"
    else:
        discovered = subprocess.run([args.engine, "--list-devices"], capture_output=True, text=True, timeout=30)
        if discovered.returncode:
            raise RuntimeError("Cannot inspect CUDA devices: " + discovered.stderr[-2000:])
        device = select_device(discovered.stdout + "\n" + discovered.stderr, args.device)
    info = metadata(args.model)
    model_name = str(info.get('general.name', '')).lower()
    # Magnum retains the Mistral NeMo metadata; this avoids hashing multi-GB weights on every launch.
    magnum = 'magnum' in model_name or 'mistral nemo' in model_name
    args.stable_template = magnum and args.template == 'auto'
    if args.cpu:
        args.kv_cache = 'f16'
    elif args.kv_cache == 'auto':
        args.kv_cache = 'q8_0' if magnum else 'f16'
    if args.context == 'auto' and not args.cpu:
        args.context = choose_context(info, args.model.stat().st_size, discovered.stdout + discovered.stderr,
                                      device, args.kv_cache, args.vram_reserve, args.parallel)
    elif args.context == 'auto':
        args.context = 4096
    os.environ.update(CONTEXT_TOKENS=str(args.context), PARALLEL=str(args.parallel),
                      LLAMA_URL=f"http://127.0.0.1:{args.engine_port}")
    from .config import Settings
    Settings()
    # Refuse to silently attach to an unrelated service already using the engine port.
    import socket
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", args.engine_port))
    with socket.socket() as probe:
        probe.bind((args.host, args.port))
    command = engine_command(args, device)
    mode = "CPU-only" if args.cpu else f"{device}; GPU layers: {args.gpu_layers}"
    print(f"Using {mode}; {args.context} context tokens per slot.")
    log = logging.getLogger('engine')
    log.setLevel(logging.INFO)
    handler = RotatingFileHandler(ROOT / 'data' / 'engine.log', maxBytes=4*1024*1024, backupCount=2, encoding='utf-8')
    log.addHandler(handler)
    child = None
    guard = None
    control = None
    reader = None
    watcher = None
    stop = threading.Event()
    finished = threading.Event()
    try:
        engine_environment = os.environ.copy()
        if args.cpu:
            engine_environment['CUDA_VISIBLE_DEVICES'] = '-1'
        child = subprocess.Popen(command, env=engine_environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                 encoding='utf-8', errors='replace')
        def drain():
            for line in child.stdout:
                log.info(line.rstrip())
                if not args.managed:
                    print(line, end='', flush=True)
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        owner = psutil.Process()
        engine = psutil.Process(child.pid)
        guard = subprocess.Popen([sys.executable, '-m', 'inference.guard', str(owner.pid), str(owner.create_time()),
                                  str(engine.pid), str(engine.create_time())], cwd=ROOT.parent,
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                  creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        state = {'pid': owner.pid, 'created': owner.create_time(), 'engine_pid': child.pid,
                 'engine_created': engine.create_time(), 'cpu_only': args.cpu, 'guard_pid': guard.pid,
                 'guard_created': psutil.Process(guard.pid).create_time(), 'root': str(ROOT.parent),
                 'api_port': args.port, 'engine_port': args.engine_port, 'context': args.context,
                 'kv_cache': args.kv_cache, 'stable_template': args.stable_template, 'status': 'loading'}
        control = Control(state, stop)
        deadline = time.monotonic() + args.startup_timeout
        while time.monotonic() < deadline:
            if stop.is_set():
                return
            if child.poll() is not None:
                raise RuntimeError(f"llama-server exited with status {child.returncode}")
            try:
                with urllib.request.urlopen(os.environ["LLAMA_URL"] + "/health", timeout=2) as response:
                    if response.status == 200:
                        break
            except OSError:
                time.sleep(0.5)
        else:
            raise TimeoutError("Model load timed out")
        import uvicorn
        from .app import create_app
        config = uvicorn.Config(create_app(), host=args.host, port=args.port, workers=1,
                    limit_concurrency=64, timeout_keep_alive=5, timeout_graceful_shutdown=15,
                    access_log=False, log_config=None if args.managed else uvicorn.config.LOGGING_CONFIG)
        server = uvicorn.Server(config)
        def monitor():
            while not finished.wait(0.2):
                if child.poll() is not None or guard.poll() is not None:
                    stop.set()
                if stop.is_set():
                    state['status'] = 'stopping'
                    server.should_exit = True
                    return
                if server.started and state['status'] != 'ready':
                    state['status'] = 'ready'
                    control.save()
        watcher = threading.Thread(target=monitor, daemon=True)
        watcher.start()
        server.run()
    finally:
        finished.set()
        if watcher:
            watcher.join(timeout=2)
        if child is not None and child.poll() is None:
            try:
                terminate_tree(psutil.Process(child.pid))
            except psutil.Error:
                pass
            child.wait(timeout=10)
        if guard:
            try:
                guard.wait(timeout=3)
            except subprocess.TimeoutExpired:
                guard.terminate()
                guard.wait(timeout=3)
        if reader:
            reader.join(timeout=3)
        if child and child.stdout:
            child.stdout.close()
        if control:
            control.close()
        log.removeHandler(handler)
        handler.close()


def main():
    parser, args = arguments()
    with ownership():
        if args.managed:
            handler = RotatingFileHandler(ROOT / 'data' / 'backend.log', maxBytes=2*1024*1024, backupCount=2, encoding='utf-8')
            logging.basicConfig(level=logging.INFO, handlers=[handler])
        try:
            serve(parser, args)
        except KeyboardInterrupt:
            pass
        except Exception:
            logging.exception('Backend failed')
            raise


if __name__ == "__main__":
    main()
