# Opus CI Results

## Performance

Up to **209.2× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 20.1 ms | **4.36 ms** | **7.2× faster** (31.35ms) | **1.3× faster** (5.70ms) | **30.1× faster** (131.15ms) |
| state | 10.6 ms | **4.29 ms** | **5.0× faster** (21.37ms) | **1.3× faster** (5.72ms) | **19.7× faster** (84.77ms) |
| branch | 10.4 ms | **4.47 ms** | **6.9× faster** (30.94ms) | **4.7× faster** (20.87ms) | **11.5× faster** (51.23ms) |
| general | 9.4 ms | **21.70 ms** | 1.1× slower (19.00ms) | 1.2× slower (18.08ms) | **2.2× faster** (48.28ms) |
| hft | 10.4 ms | **427.16 ms** | 3.0× slower (141.46ms) | 3.2× slower (131.95ms) | 1.1× slower (400.29ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42395720972) · Source: `aaab0e807ad9`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42395720972) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42395720972) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42395720972) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42395720972) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42395720972) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42395720972) · Source: `aaab0e807ad9`.
