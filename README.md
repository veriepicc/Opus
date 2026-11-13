# Sable CI Results

## Performance

Up to **19.2× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 17.4 ms | **4.37 ms** | **7.1× faster** (31.10ms) | **1.3× faster** (5.63ms) | — |
| state | 8.6 ms | **4.38 ms** | **4.9× faster** (21.64ms) | **1.3× faster** (5.71ms) | — |
| branch | 9.1 ms | **4.50 ms** | **9.2× faster** (41.49ms) | **4.5× faster** (20.31ms) | — |
| general | 12.2 ms | **26.81 ms** | 1.4× slower (18.82ms) | 1.4× slower (19.34ms) | — |
| hft | 9.9 ms | **189.21 ms** | 1.3× slower (140.87ms) | 1.4× slower (131.72ms) | — |
| math_vec | 9.8 ms | **119.31 ms** | **1.2× faster** (140.92ms) | **1.2× faster** (140.03ms) | — |
| state_vec | 10.2 ms | **165.61 ms** | **1.2× faster** (202.15ms) | **1.3× faster** (212.26ms) | — |
| branch_vec | 10.2 ms | **121.41 ms** | **1.2× faster** (150.28ms) | **1.1× faster** (137.76ms) | — |
| general_vec | 10.3 ms | **85.36 ms** | **1.3× faster** (108.05ms) | **1.3× faster** (106.89ms) | — |
| hft_vec | 9.7 ms | **102.26 ms** | **1.1× faster** (113.68ms) | tied (107.21ms) | — |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43498164967) · Source: `9c92b6d6de5d`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43498164967) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43498164967) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43498164967) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43498164967) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43498164967) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43498164967) · Source: `9c92b6d6de5d`.
