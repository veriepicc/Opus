# Opus CI Results

## Performance

Up to **309.7× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 10.8 ms | **2.86 ms** | **8.0× faster** (22.87ms) | **1.3× faster** (3.85ms) | **27.9× faster** (79.95ms) |
| state | 6.7 ms | **2.86 ms** | **5.4× faster** (15.54ms) | **1.3× faster** (3.76ms) | **17.9× faster** (50.99ms) |
| branch | 7.1 ms | **2.94 ms** | **5.4× faster** (15.78ms) | **4.8× faster** (14.14ms) | **10.8× faster** (31.66ms) |
| general | 7.4 ms | **12.79 ms** | 1.0× slower (12.59ms) | 1.0× slower (12.66ms) | **2.1× faster** (26.72ms) |
| hft | 8.0 ms | **153.98 ms** | 1.6× slower (95.43ms) | 1.8× slower (87.26ms) | **1.1× faster** (172.77ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42392835706) · Source: `f43be582229d`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42392835706) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42392835706) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42392835706) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42392835706) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42392835706) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42392835706) · Source: `f43be582229d`.
