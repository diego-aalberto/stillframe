# Add your LLM weights

## Selected and installed model

**[Anthracite Magnum v4 12B](https://huggingface.co/anthracite-org/magnum-v4-12b-gguf), Q4_K_M**, is installed as `model.gguf` (7,477,207,936 bytes, about 6.96 GiB). It is a creative-writing/roleplay fine-tune of Mistral NeMo Instruct. The publisher supplies this quantization directly. Its published training configuration uses 32,768-token sequences; the base advertises 128K, but that does not establish equal fine-tune quality at 128K. This service starts at 4K for ordinary gaming PCs; try `--context 8192` or `16384` only when memory permits.

The selected release emphasizes prose and includes no-refusal training sources. It is a candidate for mature roleplay; the local tests assess ordinary fictional dialogue and continuity, not an explicit-content benchmark. Style and behavior still depend on prompting and sampling. A 12B model also will not consistently match a frontier hosted model.

Pinned revision: `24c7243deb03204573cdaf0909804ca098f420db`.

SHA256: `0202eca3c556279987d6202130e64ecc93ed365e37f7ca1b1673c7049fc28f31`.

Redownload on another machine with `python -m inference.download --model`. Source URLs, file sizes and hashes are recorded in `inference/assets.json`. No login or paid model API is required for this selected download. To replace it, keep the existing file under another name and pass your new model with `--model`; the downloader refuses to overwrite a different existing `model.gguf`.

## Commercial-use findings

The [fine-tune](https://huggingface.co/anthracite-org/magnum-v4-12b), its official GGUF, [Mistral NeMo Instruct](https://huggingface.co/mistralai/Mistral-Nemo-Instruct-2407), and the base declare **Apache-2.0**. That license permits commercial use and redistribution without a model royalty or separate permission request, subject to its terms. For redistribution, retain the license and applicable attribution/NOTICE material and mark your modifications. It does not grant trademark rights or a non-infringement warranty. See the [Apache license](https://www.apache.org/licenses/LICENSE-2.0).

**This is a permissive published weight license, not complete training-data clearance.** The metadata audit found an MIT label for one of six listed fine-tuning datasets; five lacked an explicit license in the returned metadata. The sources include synthetic Claude-derived material. This does not itself establish that use of the weights is prohibited, but the available evidence is insufficient to promise zero permission or provenance issues for a commercial launch. A commercial review should resolve that remaining provenance question with the publisher. No publisher has been contacted on your behalf.

Copies of the Apache license, llama.cpp's MIT license, publisher model card, and retrieved upstream metadata are preserved in `inference/licenses/`. CUDA runtime redistribution has NVIDIA's separate terms; keeping the binaries local is different from shipping them in your product. Check those terms before packaging CUDA libraries.

Other candidates reviewed: Sao10K Lyra v4 has **CC-BY-NC-4.0**, so it was rejected for this commercial objective. Gryphe Pantheon RP 1.6 is Apache-labeled and specializes in personas, but was not downloaded or benchmarked. Magnum was selected for general prose, an official quantization, and the simpler base-weight license chain. None of these cards alone proves complete training-data rights.

## Bring your own model

Place a **GGUF text instruction/chat model** supported by your llama.cpp release in this folder. The default location is:

```text
models/model.gguf
```

You may keep the original filename and pass it to the launcher:

```powershell
.venv\Scripts\python -m inference.run --model "models\your-model-Q4_K_M.gguf"
```

For split GGUF weights, keep all shards together with their original names and pass the first shard, such as `model-00001-of-00003.gguf`. Do not concatenate shards or rename only one. Follow the model publisher's instructions and verify its checksum if supplied.

## Choosing a 12–20B configuration

The target is ordinary NVIDIA gaming PCs on Windows/Linux, including modest systems. Start with a 12–14B Q4_K_M model, 4,096 context tokens and one generation slot. Automatic fitting adjusts GPU offloading to available VRAM and leaves a configurable 1 GiB reserve for the desktop. Architecture, vocabulary, quantization and KV cache dimensions still affect whether a configuration fits.

| GPU VRAM | Practical starting point, subject to model-specific testing |
| --- | --- |
| 4–6 GB | 12B Q4, 2K–4K context, substantial CPU offload; latency may be high |
| 8 GB | 12–14B Q4, 4K context; some CPU offload may be needed |
| 10–12 GB | 12–14B Q4; 20B Q4 may need CPU offload |
| 16 GB or more | More room for 20B Q4 and longer context; measure actual usage |

These are planning estimates, not tested guarantees or fixed hardware profiles. The engine fits the actual model rather than using this table to guess layer counts. Prefer 32 GB system RAM for 12–20B models; 16 GB is a constrained starting point for smaller quantized models with other applications closed. Large offloads, long context and conversion may require more. SSD storage helps loading, but paging cannot substitute for sufficient RAM. A smaller model may be necessary on very modest machines if interactive speed matters.

Pure 4-bit storage is approximately 6 GB for 12B or 10 GB for 20B parameters, before quantization metadata, unquantized tensors, KV cache and compute buffers. FP16 weights alone are approximately 24–40 GB. CPU offloading requires sufficient system RAM and usually increases latency. No software optimization can guarantee fast 20B inference on every low-end gaming PC.

If loading fails due to GPU memory, first keep automatic fitting and reduce context or increase its reserve:

```powershell
.venv\Scripts\python -m inference.run --model models\model.gguf --context 2048 --vram-reserve 1536
```

You can override automatic fitting with `--gpu-layers 20`, or use `--kv-cache q8_0` on models/GPUs supporting Flash Attention to reduce KV memory. Manual layer counts disable fitting and require measurement. Stay within the model's supported context length. The launcher and API share the same context setting. A supported NVIDIA GPU, driver and CUDA engine build are required; AMD, Intel and Apple GPU backends are outside the current server scope.

## Required model metadata

The GGUF should include its tokenizer, special tokens and correct chat template. The API uses that template and tokenizer to construct the exact inference prompt. Use an instruction-tuned text model whose template accepts a system message and alternating user/assistant messages. Base models without a chat template are not plug-and-play chat assistants.

This backbone uses native text completion after applying the chat template. Tool calling, image/audio input and model-specific reasoning-channel parsing are not implemented. Prefer a non-thinking instruction model or a model configured to answer directly; otherwise reasoning text may appear in replies and summaries. Confirm template behavior with a short real conversation before using long-term memory.

## If your weights are Safetensors

Renaming `.safetensors`, `.bin` or `.pt` to `.gguf` does not convert them. Use the matching llama.cpp checkout's `convert_hf_to_gguf.py` and `llama-quantize` tools, following its architecture-specific instructions. Conversion needs the complete original model directory, including tokenizer/config files, and substantial disk/RAM capacity. No conversion or download runs automatically here.

See the official [llama.cpp model preparation guide](https://github.com/ggml-org/llama.cpp#prepare-and-quantize) and [releases](https://github.com/ggml-org/llama.cpp/releases). Choose an engine release that supports your exact architecture; retain its release identifier alongside your model notes for reproducibility.

Model files are ignored by Git. The optional downloader uses pinned public assets; it never runs unless invoked. No credentials are included.
