"""Preserve source metadata and license texts for the selected third-party assets."""
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parent


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "local-inference-setup/1.0"}), timeout=30) as response:
        return response.read()


def main():
    target = ROOT / "licenses"
    target.mkdir(exist_ok=True)
    manifest = json.loads((ROOT / "assets.json").read_text(encoding="utf-8"))
    sources = {
        "Apache-2.0.txt": "https://www.apache.org/licenses/LICENSE-2.0.txt",
        "llama.cpp-LICENSE.txt": "https://raw.githubusercontent.com/ggml-org/llama.cpp/b10930/LICENSE",
        "Magnum-model-card.txt": "https://huggingface.co/anthracite-org/magnum-v4-12b-gguf/resolve/24c7243deb03204573cdaf0909804ca098f420db/README.md",
    }
    for filename, url in sources.items():
        (target / filename).write_bytes(fetch(url))
    record = {"sources": sources, "assets": manifest, "models": {}, "datasets": {}}
    for repo in ["anthracite-org/magnum-v4-12b", "mistralai/Mistral-Nemo-Instruct-2407", "mistralai/Mistral-Nemo-Base-2407"]:
        data = json.loads(fetch("https://huggingface.co/api/models/" + repo))
        record["models"][repo] = {k: data.get(k) for k in ("sha", "cardData", "gated")}
    data = json.loads(fetch("https://huggingface.co/api/models/" + manifest["model"]["repository"]))
    for repo in data.get("cardData", {}).get("datasets", []):
        try:
            dataset = json.loads(fetch("https://huggingface.co/api/datasets/" + repo))
            record["datasets"][repo] = {k: dataset.get(k) for k in ("sha", "cardData", "gated")}
        except OSError as exc:
            record["datasets"][repo] = {"unresolved": str(exc)}
    (target / "provenance.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
    print(json.dumps({"model_licenses": {k: v["cardData"].get("license") for k, v in record["models"].items()},
                      "dataset_licenses": {k: (v.get("cardData") or {}).get("license", "UNRESOLVED") for k, v in record["datasets"].items()}}, indent=2))


if __name__ == "__main__":
    main()
