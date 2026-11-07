# Opus CI Results

## Performance

Up to **172.1× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 18.4 ms | **4.73 ms** | **6.4× faster** (30.50ms) | **1.2× faster** (5.52ms) | **27.6× faster** (130.70ms) |
| state | 8.4 ms | **4.61 ms** | **4.5× faster** (20.85ms) | **1.2× faster** (5.43ms) | **18.2× faster** (83.76ms) |
| branch | 8.2 ms | **4.19 ms** | **7.3× faster** (30.43ms) | **4.8× faster** (20.09ms) | **12.0× faster** (50.41ms) |
| general | 8.9 ms | **21.25 ms** | 1.1× slower (18.54ms) | 1.2× slower (17.74ms) | **2.2× faster** (47.43ms) |
| hft | 11.0 ms | **427.83 ms** | 3.0× slower (140.72ms) | 3.3× slower (131.20ms) | 1.1× slower (400.59ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42402350788) · Source: `1cf2e032fb77`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402350788) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402350788) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402350788) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402350788) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402350788) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42402350788) · Source: `1cf2e032fb77`.
