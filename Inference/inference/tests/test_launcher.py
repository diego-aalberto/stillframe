import pytest

from inference.run import arguments, engine_command, select_device


def test_portable_defaults_leave_layers_to_engine():
    _, args = arguments([])
    command = engine_command(args, "CUDA0")
    assert "--n-gpu-layers" not in command
    assert command[command.index("--fit") + 1] == "on"
    assert command[command.index("--ctx-size") + 1] == "4096"
    assert command[command.index("--cache-ram") + 1] == "0"
    assert command[command.index("--flash-attn") + 1] == "auto"


def test_cpu_mode_never_requests_gpu():
    _, args = arguments(["--cpu"])
    command = engine_command(args, "none")
    assert command[command.index("--device") + 1] == "none"
    assert command[command.index("--n-gpu-layers") + 1] == "0"
    assert command[command.index("--fit") + 1] == "off"
    assert command[command.index("--flash-attn") + 1] == "off"


def test_manual_override_and_total_context():
    _, args = arguments(["--parallel", "2", "--gpu-layers", "20", "--kv-cache", "q8_0"])
    command = engine_command(args, "CUDA1")
    assert command[command.index("--ctx-size") + 1] == "8192"
    assert command[command.index("--fit") + 1] == "off"
    assert command[command.index("--n-gpu-layers") + 1] == "20"
    assert command[command.index("--flash-attn") + 1] == "on"


def test_nvidia_device_selection_and_unsupported_backend():
    devices = "Available devices:\n  CUDA0: NVIDIA Example (6144 MiB)\n  CUDA1: NVIDIA Other (8192 MiB)"
    assert select_device(devices) == "CUDA0"
    assert select_device(devices, "CUDA1") == "CUDA1"
    with pytest.raises(RuntimeError, match="not available"):
        select_device(devices, "CUDA2")
    with pytest.raises(RuntimeError, match="No CUDA GPU"):
        select_device("Vulkan0: Other GPU")


@pytest.mark.parametrize("options", [["--gpu-layers", "0"], ["--gpu-layers", "bad"],
                                     ["--vram-reserve", "0"], ["--ubatch-size", "2048"],
                                     ["--port", "8081"], ["--startup-timeout", "0"]])
def test_invalid_launch_options(options):
    with pytest.raises(SystemExit):
        arguments(options)
