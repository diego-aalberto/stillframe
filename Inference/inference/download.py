"""Download pinned model/Windows CUDA assets, verify SHA256, and install locally."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def download(asset, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        if destination.stat().st_size == asset["size"] and digest(destination) == asset["sha256"]:
            print(f"Already verified: {destination.name}", flush=True)
            return destination
        raise RuntimeError(f"Existing file differs from manifest; refusing to overwrite {destination}")
    partial = destination.with_suffix(destination.suffix + ".part")
    for attempt in range(5):
        offset = partial.stat().st_size if partial.exists() else 0
        if offset == asset["size"]:
            break
        if offset > asset["size"]:
            raise RuntimeError(f"Oversized partial download: {partial}")
        try:
            headers = {"User-Agent": "local-inference-setup/1.0"}
            if offset:
                headers["Range"] = f"bytes={offset}-"
            req = urllib.request.Request(asset["url"], headers=headers)
            with urllib.request.urlopen(req, timeout=60) as response:
                if offset and response.status != 206:
                    raise RuntimeError("Server refused resume; keep or remove the .part file before retrying")
                if offset and not response.headers.get("Content-Range", "").startswith(f"bytes {offset}-"):
                    raise RuntimeError("Invalid range response")
                with partial.open("ab" if offset else "wb") as file:
                    report = time.monotonic()
                    while chunk := response.read(4 * 1024 * 1024):
                        file.write(chunk)
                        offset += len(chunk)
                        if offset > asset["size"]:
                            raise RuntimeError("Download larger than expected")
                        if time.monotonic() - report >= 10:
                            print(f"{destination.name}: {offset / asset['size']:.0%} ({offset / 1e9:.2f} GB)", flush=True)
                            report = time.monotonic()
            if offset == asset["size"]:
                break
        except OSError as exc:
            if attempt == 4:
                raise
            print(f"Download interrupted ({exc}); resuming", flush=True)
            time.sleep(2)
    if partial.stat().st_size != asset["size"] or digest(partial) != asset["sha256"]:
        raise RuntimeError(f"Checksum/size mismatch: {partial}")
    partial.rename(destination)
    print(f"SHA256 verified: {destination.name}", flush=True)
    return destination


def install_engine(archives):
    target = ROOT / "bin"
    target.mkdir(exist_ok=True)
    for archive in archives:
        with zipfile.ZipFile(archive) as package:
            for info in package.infolist():
                name = Path(info.filename).name
                if info.is_dir() or not (name.lower().endswith((".dll", ".exe")) or "license" in name.lower() or "notice" in name.lower()):
                    continue
                if not name or name in (".", ".."):
                    raise RuntimeError("Invalid archive filename")
                # Flatten binary distributions. No archive-controlled directory is used.
                output = target / name
                with package.open(info) as source, output.open("wb") as file:
                    shutil.copyfileobj(source, file)
    if not (target / "llama-server.exe").exists():
        raise RuntimeError("Archive did not contain llama-server.exe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", action="store_true")
    parser.add_argument("--windows-engine", action="store_true", help="Pinned CUDA 13.3 build; check GPU/driver support")
    args = parser.parse_args()
    if not args.model and not args.windows_engine:
        parser.error("Choose --model and/or --windows-engine")
    if args.windows_engine and os.name != "nt":
        parser.error("Install a compatible Linux CUDA engine separately; --model is cross-platform")
    manifest = json.loads((ROOT / "assets.json").read_text(encoding="utf-8"))
    jobs = []
    if args.model:
        jobs.append((manifest["model"], ROOT.parent / "models" / "model.gguf"))
    if args.windows_engine:
        jobs.extend((a, ROOT / "downloads" / a["filename"]) for a in manifest["windows_engine"])
    with ThreadPoolExecutor(max_workers=3) as pool:
        downloaded = list(pool.map(lambda pair: download(*pair), jobs))
    if args.windows_engine:
        install_engine([p for p in downloaded if p.suffix == ".zip"])
        print("Windows CUDA engine installed in inference/bin", flush=True)


if __name__ == "__main__":
    main()
