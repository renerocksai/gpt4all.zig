<h1 align="center">GPT4All.zig</h1>
<p align="center">An assistant-style large language model with ~800k GPT-3.5-Turbo Generations based on LLaMa</p>

# Try it yourself

Here's how to get started with the CPU quantized GPT4All model checkpoint:

0. Make sure you have Zig master installed. Download from [here](https://ziglang.org/download/)
1. Download the `gpt4all-lora-quantized.bin` file from [Direct Link](https://the-eye.eu/public/AI/models/nomic-ai/gpt4all/gpt4all-lora-quantized.bin) or [[Torrent-Magnet]](https://tinyurl.com/gpt4all-lora-quantized).
2. Clone this repository
3. Compile with `zig build -Doptimize=ReleaseFast`
4. Run with `./zig-out/bin/chat

If the downloaded model file is located somewhere else, start with:

```shell
$ ./zig-out/bin/chat -m /path/to/model.bin
```

**Note:** This work is based on the excellent ones done by Nomic.ai:
- [GPT4All](https://github.com/nomic-ai/gpt4all) : 
- [gpt4all.cpp](https://github.com/zanussbaum/gpt4all.cpp) : 
