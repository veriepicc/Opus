# Sable CI Results

## Performance

Up to **24.6× faster compilation** than MSVC · **5.0 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 16.5 ms | **3.49 ms** | **7.9× faster** (27.53ms) | **1.3× faster** (4.64ms) | **33.0× faster** (115.19ms) |
| state | 8.1 ms | **3.51 ms** | **5.4× faster** (18.89ms) | **1.4× faster** (4.79ms) | **21.1× faster** (74.01ms) |
| branch | 8.0 ms | **3.58 ms** | **7.7× faster** (27.69ms) | **4.9× faster** (17.72ms) | **11.3× faster** (40.53ms) |
| general | 8.6 ms | **20.89 ms** | 1.3× slower (16.55ms) | 1.2× slower (17.18ms) | **1.9× faster** (39.07ms) |
| hft | 9.1 ms | **148.52 ms** | 1.2× slower (120.22ms) | 1.3× slower (117.26ms) | **1.7× faster** (258.61ms) |
| math_vec | 8.0 ms | **26.42 ms** | **4.6× faster** (121.94ms) | **4.6× faster** (121.74ms) | **7.0× faster** (185.44ms) |
| state_vec | 8.2 ms | **26.63 ms** | **6.8× faster** (181.09ms) | **7.1× faster** (189.35ms) | **12.2× faster** (326.05ms) |
| branch_vec | 8.0 ms | **27.26 ms** | **4.5× faster** (122.89ms) | **4.6× faster** (124.33ms) | **7.9× faster** (214.17ms) |
| general_vec | 8.2 ms | **15.06 ms** | **6.2× faster** (93.47ms) | **6.4× faster** (97.07ms) | **11.6× faster** (174.88ms) |
| hft_vec | 9.3 ms | **15.42 ms** | **6.3× faster** (96.68ms) | **6.5× faster** (100.29ms) | **11.5× faster** (177.19ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43531134665) · Source: `6609a5c208c1`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531134665) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43531134665) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43531134665) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43531134665) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43531134665) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43531134665) · Source: `6609a5c208c1`.
