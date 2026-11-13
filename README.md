# Sable CI Results

## Performance

Up to **21.6× faster compilation** than MSVC · **5.0 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 22.5 ms | **5.03 ms** | **6.4× faster** (32.37ms) | **1.3× faster** (6.73ms) | **26.2× faster** (131.89ms) |
| state | 10.0 ms | **4.97 ms** | **4.6× faster** (22.95ms) | **1.4× faster** (6.74ms) | **17.3× faster** (85.79ms) |
| branch | 9.9 ms | **5.08 ms** | **6.4× faster** (32.47ms) | **4.2× faster** (21.53ms) | **10.2× faster** (51.98ms) |
| general | 10.7 ms | **24.83 ms** | 1.2× slower (20.16ms) | 1.2× slower (20.97ms) | **2.0× faster** (49.50ms) |
| hft | 11.1 ms | **192.65 ms** | 1.4× slower (141.90ms) | 1.5× slower (132.10ms) | **2.1× faster** (401.68ms) |
| math_vec | 11.2 ms | **50.63 ms** | **2.8× faster** (139.72ms) | **2.8× faster** (141.97ms) | **4.2× faster** (211.20ms) |
| state_vec | 10.5 ms | **52.61 ms** | **3.6× faster** (187.96ms) | **4.0× faster** (212.91ms) | **6.9× faster** (361.03ms) |
| branch_vec | 11.3 ms | **52.93 ms** | **2.7× faster** (140.93ms) | **2.6× faster** (137.82ms) | **4.8× faster** (251.89ms) |
| general_vec | 10.9 ms | **29.41 ms** | **3.8× faster** (111.50ms) | **3.8× faster** (110.50ms) | **6.7× faster** (195.65ms) |
| hft_vec | 11.3 ms | **28.51 ms** | **4.1× faster** (115.62ms) | **3.9× faster** (109.92ms) | **6.9× faster** (197.22ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43529715163) · Source: `da659d44abe3`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43529715163) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43529715163) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43529715163) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43529715163) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43529715163) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43529715163) · Source: `da659d44abe3`.
