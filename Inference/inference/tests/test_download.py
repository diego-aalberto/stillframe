import hashlib
import io
import zipfile

import pytest

from inference import download as downloader


class Response(io.BytesIO):
    def __init__(self, data, status=200, headers=None):
        super().__init__(data)
        self.status = status
        self.headers = headers or {}


def asset(data):
    return {"url": "https://example.test/model", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def test_download_verification_and_reuse(tmp_path, monkeypatch):
    data = b"model data"
    calls = []
    def open_url(*args, **kwargs):
        calls.append(args)
        return Response(data)
    monkeypatch.setattr(downloader.urllib.request, "urlopen", open_url)
    path = tmp_path / "model.gguf"
    assert downloader.download(asset(data), path).read_bytes() == data
    downloader.download(asset(data), path)
    assert len(calls) == 1


def test_resume_exact_range(tmp_path, monkeypatch):
    data = b"model data"
    path = tmp_path / "model.gguf"
    path.with_suffix(".gguf.part").write_bytes(data[:3])
    def open_url(req, **kwargs):
        assert req.get_header("Range") == "bytes=3-"
        return Response(data[3:], 206, {"Content-Range": "bytes 3-9/10"})
    monkeypatch.setattr(downloader.urllib.request, "urlopen", open_url)
    assert downloader.download(asset(data), path).read_bytes() == data


def test_bad_digest_never_promotes_partial(tmp_path, monkeypatch):
    monkeypatch.setattr(downloader.urllib.request, "urlopen", lambda *a, **k: Response(b"wrong"))
    path = tmp_path / "model.gguf"
    with pytest.raises(RuntimeError, match="Checksum"):
        downloader.download(asset(b"right"), path)
    assert not path.exists()


def test_existing_model_is_not_overwritten(tmp_path):
    path = tmp_path / "model.gguf"
    path.write_bytes(b"my existing model")
    with pytest.raises(RuntimeError, match="refusing to overwrite"):
        downloader.download(asset(b"other"), path)
    assert path.read_bytes() == b"my existing model"


def test_engine_extracts_only_selected_files_under_bin(tmp_path, monkeypatch):
    monkeypatch.setattr(downloader, "ROOT", tmp_path)
    archive = tmp_path / "engine.zip"
    with zipfile.ZipFile(archive, "w") as package:
        package.writestr("bin/llama-server.exe", b"engine")
        package.writestr("../../escape.dll", b"dll")
        package.writestr("README.md", b"not installed")
    downloader.install_engine([archive])
    assert (tmp_path / "bin" / "llama-server.exe").read_bytes() == b"engine"
    assert (tmp_path / "bin" / "escape.dll").read_bytes() == b"dll"
    assert not (tmp_path / "bin" / "README.md").exists()
