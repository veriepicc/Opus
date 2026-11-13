# Sable CI Results

## Performance

Up to **19.4× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 13.1 ms | **4.46 ms** | **7.0× faster** (31.37ms) | **1.3× faster** (5.72ms) | **29.4× faster** (131.29ms) |
| state | 9.4 ms | **4.30 ms** | **5.2× faster** (22.22ms) | **1.3× faster** (5.53ms) | **19.7× faster** (84.58ms) |
| branch | 9.3 ms | **4.43 ms** | **7.1× faster** (31.57ms) | **4.6× faster** (20.46ms) | **11.6× faster** (51.53ms) |
| general | 10.0 ms | **24.12 ms** | 1.3× slower (18.87ms) | 1.2× slower (19.54ms) | **2.0× faster** (48.84ms) |
| hft | 11.0 ms | **182.36 ms** | 1.3× slower (141.39ms) | 1.4× slower (131.58ms) | **2.2× faster** (400.65ms) |
| math_vec | 9.5 ms | **112.59 ms** | **1.2× faster** (132.57ms) | **1.2× faster** (133.76ms) | **1.9× faster** (209.48ms) |
| state_vec | 9.4 ms | **153.25 ms** | **1.3× faster** (191.57ms) | **1.3× faster** (199.35ms) | **2.3× faster** (349.83ms) |
| branch_vec | 11.5 ms | **120.56 ms** | **1.2× faster** (142.90ms) | **1.1× faster** (134.48ms) | **2.0× faster** (241.80ms) |
| general_vec | 9.8 ms | **81.23 ms** | **1.3× faster** (104.51ms) | **1.2× faster** (100.37ms) | **2.4× faster** (191.19ms) |
| hft_vec | 10.2 ms | **95.69 ms** | **1.1× faster** (108.31ms) | **1.1× faster** (101.85ms) | **2.0× faster** (193.80ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43499690339) · Source: `9d28159c90dd`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43499690339) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43499690339) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43499690339) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43499690339) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43499690339) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43499690339) · Source: `9d28159c90dd`.
