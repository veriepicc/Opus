# Sable CI Results

## Performance

Up to **202.5× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 19.7 ms | **4.59 ms** | **6.8× faster** (31.36ms) | **1.2× faster** (5.63ms) | **28.7× faster** (132.05ms) |
| state | 9.6 ms | **4.40 ms** | **5.1× faster** (22.30ms) | **1.3× faster** (5.92ms) | **19.3× faster** (84.75ms) |
| branch | 10.0 ms | **4.45 ms** | **7.1× faster** (31.57ms) | **4.7× faster** (20.85ms) | **11.5× faster** (51.27ms) |
| general | 9.5 ms | **22.20 ms** | 1.1× slower (19.39ms) | 1.2× slower (18.45ms) | **2.2× faster** (48.75ms) |
| hft | 10.5 ms | **427.51 ms** | 3.0× slower (141.52ms) | 3.2× slower (132.06ms) | 1.1× slower (399.85ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/42425082235) · Source: `82fb8eb33a4b`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/42425082235) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42425082235) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42425082235) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42425082235) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42425082235) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/42425082235) · Source: `82fb8eb33a4b`.
