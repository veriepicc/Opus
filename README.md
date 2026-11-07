# Opus CI Results

## Performance

Up to **367.1× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 9.6 ms | **2.71 ms** | **7.8× faster** (21.19ms) | **1.3× faster** (3.50ms) | **27.6× faster** (74.56ms) |
| state | 5.3 ms | **2.62 ms** | **5.5× faster** (14.48ms) | **1.4× faster** (3.62ms) | **18.2× faster** (47.73ms) |
| branch | 5.3 ms | **2.63 ms** | **5.1× faster** (13.51ms) | **4.8× faster** (12.49ms) | **10.6× faster** (27.75ms) |
| general | 6.1 ms | **12.10 ms** | 1.1× slower (10.82ms) | tied (11.74ms) | **2.0× faster** (23.67ms) |
| hft | 6.0 ms | **143.97 ms** | 1.6× slower (88.70ms) | 1.8× slower (81.30ms) | **1.1× faster** (165.53ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42403711803) · Source: `3df5e31bb948`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42403711803) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42403711803) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42403711803) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42403711803) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42403711803) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42403711803) · Source: `3df5e31bb948`.
