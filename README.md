# Sable CI Results

## Performance

Up to **374.3× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 12.3 ms | **3.67 ms** | **7.5× faster** (27.45ms) | **1.2× faster** (4.49ms) | **31.3× faster** (114.76ms) |
| state | 7.6 ms | **3.42 ms** | **5.5× faster** (18.85ms) | **1.4× faster** (4.94ms) | **21.5× faster** (73.74ms) |
| branch | 7.7 ms | **3.45 ms** | **8.0× faster** (27.40ms) | **5.2× faster** (17.94ms) | **11.7× faster** (40.18ms) |
| general | 8.1 ms | **17.22 ms** | tied (16.61ms) | 1.1× slower (16.05ms) | **2.2× faster** (38.35ms) |
| hft | 8.6 ms | **219.28 ms** | 1.8× slower (120.33ms) | 2.0× slower (112.23ms) | **1.2× faster** (258.94ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/42423385826) · Source: `2cabe210e1fd`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/42423385826) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42423385826) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42423385826) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42423385826) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42423385826) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/42423385826) · Source: `2cabe210e1fd`.
