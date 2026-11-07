# Opus CI Results

## Performance

Up to **290.2× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 18.1 ms | **4.12 ms** | **4.2× faster** (17.13ms) | **1.3× faster** (5.44ms) | **19.1× faster** (78.82ms) |
| state | 9.2 ms | **4.01 ms** | **4.3× faster** (17.44ms) | **1.3× faster** (5.39ms) | **12.2× faster** (48.87ms) |
| branch | 7.8 ms | **3.63 ms** | **4.1× faster** (15.02ms) | **4.1× faster** (14.71ms) | **8.9× faster** (32.37ms) |
| general | 7.8 ms | **14.26 ms** | 1.3× slower (11.28ms) | 1.2× slower (12.11ms) | **2.0× faster** (28.49ms) |
| hft | 8.2 ms | **189.97 ms** | 2.0× slower (93.68ms) | 2.2× slower (87.07ms) | **1.1× faster** (207.35ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42394055315) · Source: `b5078ce6b763`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42394055315) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42394055315) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42394055315) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42394055315) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42394055315) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42394055315) · Source: `b5078ce6b763`.
