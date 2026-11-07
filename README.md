# Opus CI Results

## Performance

Up to **306.1× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 20.8 ms | **4.66 ms** | **6.7× faster** (31.11ms) | **1.2× faster** (5.81ms) | **28.3× faster** (131.96ms) |
| state | 9.3 ms | **4.42 ms** | **4.8× faster** (21.44ms) | **1.3× faster** (5.92ms) | **19.2× faster** (84.74ms) |
| branch | 9.0 ms | **4.47 ms** | **6.9× faster** (30.99ms) | **4.6× faster** (20.51ms) | **11.4× faster** (51.17ms) |
| general | 11.0 ms | **21.82 ms** | 1.2× slower (18.85ms) | 1.2× slower (18.29ms) | **2.2× faster** (48.03ms) |
| hft | 9.9 ms | **428.44 ms** | 3.0× slower (141.42ms) | 3.2× slower (132.71ms) | tied (407.96ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42397766021) · Source: `4b4423fdd042`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397766021) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397766021) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397766021) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397766021) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397766021) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42397766021) · Source: `4b4423fdd042`.
