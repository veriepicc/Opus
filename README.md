# Opus CI Results

## Performance

Up to **310.0× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 22.4 ms | **6.25 ms** | **5.2× faster** (32.31ms) | **1.1× faster** (7.14ms) | **21.5× faster** (134.31ms) |
| state | 10.5 ms | **5.51 ms** | **4.2× faster** (23.21ms) | **1.2× faster** (6.88ms) | **15.5× faster** (85.59ms) |
| branch | 10.2 ms | **5.18 ms** | **6.4× faster** (33.00ms) | **4.3× faster** (22.18ms) | **10.1× faster** (52.34ms) |
| general | 11.0 ms | **23.19 ms** | 1.1× slower (20.87ms) | 1.1× slower (20.25ms) | **2.2× faster** (51.86ms) |
| hft | 11.5 ms | **432.53 ms** | 3.0× slower (142.31ms) | 3.2× slower (133.26ms) | 1.1× slower (403.14ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42401782794) · Source: `ef0b3a12f1a4`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42401782794) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42401782794) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42401782794) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42401782794) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42401782794) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42401782794) · Source: `ef0b3a12f1a4`.
