# Sable CI Results

## Performance

Up to **30.0× faster compilation** than MSVC · **5.0 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 9.2 ms | **2.86 ms** | **7.9× faster** (22.45ms) | **1.3× faster** (3.59ms) | **26.7× faster** (76.28ms) |
| state | 7.1 ms | **2.82 ms** | **5.4× faster** (15.14ms) | **1.3× faster** (3.64ms) | **17.5× faster** (49.21ms) |
| branch | 6.9 ms | **2.85 ms** | **5.0× faster** (14.38ms) | **4.6× faster** (13.15ms) | **9.9× faster** (28.36ms) |
| general | 7.4 ms | **13.13 ms** | 1.2× slower (11.03ms) | tied (13.45ms) | **1.9× faster** (25.51ms) |
| hft | 7.8 ms | **107.78 ms** | 1.2× slower (90.35ms) | 1.3× slower (84.55ms) | **1.5× faster** (162.50ms) |
| math_vec | 7.0 ms | **21.98 ms** | **4.5× faster** (98.90ms) | **4.5× faster** (99.07ms) | **6.6× faster** (144.32ms) |
| state_vec | 7.1 ms | **22.75 ms** | **6.0× faster** (136.73ms) | **6.7× faster** (152.44ms) | **10.2× faster** (232.09ms) |
| branch_vec | 7.1 ms | **23.29 ms** | **4.3× faster** (99.05ms) | **4.3× faster** (99.87ms) | **6.9× faster** (159.85ms) |
| general_vec | 7.0 ms | **12.59 ms** | **5.7× faster** (71.89ms) | **6.1× faster** (77.07ms) | **9.9× faster** (125.14ms) |
| hft_vec | 7.1 ms | **13.58 ms** | **5.7× faster** (77.53ms) | **5.8× faster** (78.73ms) | **9.4× faster** (128.03ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43531408198) · Source: `ea7cdc96a6e5`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531408198) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 335 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531408198) |
| Windows Server 2025 | Native x64 | 335 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531408198) |
| Windows 11 on Arm | x64 emulation | 335 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531408198) |
| Ubuntu 24.04 | Wine x64 | 335 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/43531408198) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43531408198) · Source: `ea7cdc96a6e5`.
