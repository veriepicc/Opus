# Sable CI Results

## Performance

Up to **313.8× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 14.5 ms | **4.61 ms** | **4.3× faster** (19.72ms) | **1.3× faster** (6.09ms) | **23.2× faster** (106.85ms) |
| state | 9.3 ms | **38.91 ms** | 1.7× slower (23.04ms) | 6.7× slower (5.84ms) | **1.7× faster** (64.99ms) |
| branch | 9.2 ms | **69.60 ms** | 2.1× slower (33.13ms) | 3.5× slower (19.87ms) | 1.1× slower (64.52ms) |
| general | 10.0 ms | **21.39 ms** | 1.1× slower (20.27ms) | 1.4× slower (15.73ms) | **2.0× faster** (41.77ms) |
| hft | 11.8 ms | **237.50 ms** | 1.8× slower (130.01ms) | 2.0× slower (117.16ms) | **1.1× faster** (266.34ms) |
| math_vec | 9.8 ms | **120.99 ms** | **2.0× faster** (244.83ms) | **2.0× faster** (237.83ms) | **2.7× faster** (332.34ms) |
| state_vec | 9.6 ms | **172.94 ms** | **2.1× faster** (364.85ms) | **2.1× faster** (363.38ms) | **2.7× faster** (474.94ms) |
| branch_vec | 10.1 ms | **126.11 ms** | **1.9× faster** (240.69ms) | **1.9× faster** (235.97ms) | **2.5× faster** (310.85ms) |
| general_vec | 10.0 ms | **87.74 ms** | **2.1× faster** (183.32ms) | **2.1× faster** (182.57ms) | **3.0× faster** (259.39ms) |
| hft_vec | 9.9 ms | **103.89 ms** | **1.8× faster** (192.06ms) | **1.9× faster** (193.32ms) | **2.3× faster** (238.96ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/42850410216) · Source: `18beac0a1425`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/42850410216) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42850410216) |
| Windows Server 2025 | Native x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42850410216) |
| Windows 11 on Arm | x64 emulation | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42850410216) |
| Ubuntu 24.04 | Wine x64 | 332 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42850410216) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/42850410216) · Source: `18beac0a1425`.
