# Opus CI Results

## Performance

Up to **227.3× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 17.6 ms | **3.17 ms** | **7.2× faster** (22.74ms) | **1.2× faster** (3.78ms) | **25.2× faster** (79.97ms) |
| state | 7.6 ms | **3.01 ms** | **5.4× faster** (16.37ms) | **1.3× faster** (3.88ms) | **17.4× faster** (52.29ms) |
| branch | 7.5 ms | **3.23 ms** | **4.5× faster** (14.59ms) | **4.4× faster** (14.08ms) | **8.9× faster** (28.70ms) |
| general | 7.1 ms | **13.18 ms** | 1.1× slower (11.61ms) | 1.0× slower (12.73ms) | **1.9× faster** (25.43ms) |
| hft | 7.6 ms | **159.98 ms** | 1.7× slower (92.75ms) | 1.9× slower (86.29ms) | **1.1× faster** (170.85ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42393341226) · Source: `62cecb22cbb8`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42393341226) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42393341226) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42393341226) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42393341226) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42393341226) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42393341226) · Source: `62cecb22cbb8`.
