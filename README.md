# Sable CI Results

## Performance

Up to **193.8× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 18.7 ms | **4.42 ms** | **6.9× faster** (30.49ms) | **1.2× faster** (5.51ms) | **29.7× faster** (131.29ms) |
| state | 9.0 ms | **4.28 ms** | **4.9× faster** (21.17ms) | **1.3× faster** (5.68ms) | **19.6× faster** (84.07ms) |
| branch | 9.1 ms | **4.33 ms** | **7.2× faster** (31.26ms) | **4.7× faster** (20.44ms) | **11.7× faster** (50.80ms) |
| general | 10.9 ms | **21.52 ms** | 1.2× slower (18.53ms) | 1.2× slower (17.79ms) | **2.2× faster** (47.69ms) |
| hft | 9.6 ms | **427.83 ms** | 3.0× slower (140.78ms) | 3.2× slower (131.77ms) | 1.1× slower (400.74ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/42426212342) · Source: `8a9ed2faeff7`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/42426212342) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42426212342) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42426212342) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42426212342) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42426212342) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/42426212342) · Source: `8a9ed2faeff7`.
