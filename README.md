# Sable CI Results

## Performance

Up to **18.5× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 18.8 ms | **4.35 ms** | **7.0× faster** (30.37ms) | **1.3× faster** (5.56ms) | **30.1× faster** (130.94ms) |
| state | 8.6 ms | **4.31 ms** | **4.9× faster** (21.23ms) | **1.3× faster** (5.52ms) | **19.5× faster** (83.89ms) |
| branch | 10.5 ms | **4.58 ms** | **6.7× faster** (30.56ms) | **4.3× faster** (19.87ms) | **11.1× faster** (50.77ms) |
| general | 9.5 ms | **23.34 ms** | 1.2× slower (18.92ms) | 1.1× slower (20.73ms) | **2.1× faster** (48.71ms) |
| hft | 10.1 ms | **182.06 ms** | 1.3× slower (141.46ms) | 1.4× slower (131.66ms) | **2.2× faster** (401.56ms) |
| math_vec | 10.8 ms | **115.98 ms** | **1.2× faster** (134.76ms) | **1.2× faster** (139.69ms) | **1.8× faster** (213.70ms) |
| state_vec | 10.5 ms | **156.32 ms** | **1.2× faster** (192.21ms) | **1.3× faster** (199.91ms) | **2.3× faster** (354.35ms) |
| branch_vec | 9.2 ms | **118.74 ms** | **1.2× faster** (146.72ms) | **1.1× faster** (133.67ms) | **2.0× faster** (243.28ms) |
| general_vec | 9.5 ms | **80.35 ms** | **1.3× faster** (104.67ms) | **1.3× faster** (103.29ms) | **2.4× faster** (191.94ms) |
| hft_vec | 9.4 ms | **97.86 ms** | **1.1× faster** (107.35ms) | **1.1× faster** (103.52ms) | **2.0× faster** (194.42ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43500729658) · Source: `7a39b9eab9c9`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43500729658) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43500729658) |
| Windows Server 2025 | Native x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43500729658) |
| Windows 11 on Arm | x64 emulation | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43500729658) |
| Ubuntu 24.04 | Wine x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43500729658) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43500729658) · Source: `7a39b9eab9c9`.
