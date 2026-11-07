# Opus CI Results

## Performance

Up to **192.6× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 20.3 ms | **4.58 ms** | **6.8× faster** (31.24ms) | **1.3× faster** (5.74ms) | **28.8× faster** (132.14ms) |
| state | 10.1 ms | **4.48 ms** | **5.0× faster** (22.20ms) | **1.3× faster** (5.80ms) | **19.0× faster** (85.14ms) |
| branch | 8.9 ms | **4.34 ms** | **7.6× faster** (33.00ms) | **4.8× faster** (20.90ms) | **12.0× faster** (52.17ms) |
| general | 10.9 ms | **22.35 ms** | 1.1× slower (20.38ms) | 1.2× slower (18.09ms) | **2.2× faster** (48.83ms) |
| hft | 11.5 ms | **428.71 ms** | 3.0× slower (142.12ms) | 3.2× slower (133.23ms) | 1.1× slower (405.91ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42400872440) · Source: `e70c2110e6bd`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42400872440) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42400872440) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42400872440) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42400872440) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42400872440) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42400872440) · Source: `e70c2110e6bd`.
