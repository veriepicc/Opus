# Opus CI Results

## Performance

Up to **196.8× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 19.3 ms | **4.43 ms** | **6.9× faster** (30.43ms) | **1.3× faster** (5.66ms) | **29.5× faster** (130.79ms) |
| state | 9.1 ms | **4.36 ms** | **4.9× faster** (21.18ms) | **1.3× faster** (5.60ms) | **19.2× faster** (83.84ms) |
| branch | 8.6 ms | **4.33 ms** | **7.1× faster** (30.52ms) | **4.7× faster** (20.28ms) | **11.7× faster** (50.57ms) |
| general | 9.1 ms | **21.37 ms** | 1.2× slower (18.58ms) | 1.2× slower (17.90ms) | **2.2× faster** (47.54ms) |
| hft | 10.6 ms | **426.70 ms** | 3.0× slower (141.33ms) | 3.2× slower (131.56ms) | 1.1× slower (400.54ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42398789888) · Source: `68461aeb48ca`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42398789888) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42398789888) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42398789888) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42398789888) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42398789888) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42398789888) · Source: `68461aeb48ca`.
