# Opus CI Results

## Performance

Up to **171.8× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 21.4 ms | **5.04 ms** | **6.4× faster** (32.47ms) | **1.3× faster** (6.75ms) | **26.1× faster** (131.81ms) |
| state | 12.6 ms | **5.57 ms** | **4.1× faster** (23.09ms) | **1.3× faster** (7.01ms) | **15.3× faster** (85.35ms) |
| branch | 10.8 ms | **5.48 ms** | **6.0× faster** (33.11ms) | **4.2× faster** (22.79ms) | **10.0× faster** (54.62ms) |
| general | 11.2 ms | **23.53 ms** | 1.2× slower (20.24ms) | 1.2× slower (19.99ms) | **2.1× faster** (49.98ms) |
| hft | 13.0 ms | **429.29 ms** | 3.0× slower (142.00ms) | 3.2× slower (132.93ms) | 1.1× slower (402.43ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42418165124) · Source: `53df54b229b8`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42418165124) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42418165124) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42418165124) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42418165124) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42418165124) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42418165124) · Source: `53df54b229b8`.
