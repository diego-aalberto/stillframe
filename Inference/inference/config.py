import os
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Settings:
    backend: str = os.getenv("LLAMA_URL", "http://127.0.0.1:8081")
    database: str = os.getenv("MEMORY_DB", str(ROOT / "data" / "memory.sqlite3"))
    api_key: str = os.getenv("INFERENCE_API_KEY", "")
    context: int = int(os.getenv("CONTEXT_TOKENS", "4096"))
    output: int = int(os.getenv("MAX_OUTPUT_TOKENS", "512"))
    summary: int = int(os.getenv("SUMMARY_TOKENS", "256"))
    parallel: int = int(os.getenv("PARALLEL", "1"))
    timeout: float = float(os.getenv("GENERATION_TIMEOUT", "600"))
    queue_timeout: float = float(os.getenv("QUEUE_TIMEOUT", "15"))

    def __post_init__(self):
        if self.context < 2048 or not 64 <= self.summary < self.context // 2:
            raise ValueError("Invalid context/summary token budget")
        if not 1 <= self.output < self.context // 2 or self.parallel < 1:
            raise ValueError("Invalid output/parallel setting")
        if self.timeout <= 0 or self.queue_timeout <= 0:
            raise ValueError("Timeouts must be positive")
