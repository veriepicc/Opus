# Sable CI Results

## Performance

Up to **265.8× faster compilation** than MSVC · **4.5 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 15.3 ms | **2.86 ms** | **7.9× faster** (22.71ms) | **1.5× faster** (4.17ms) | **28.5× faster** (81.59ms) |
| state | 8.3 ms | **2.91 ms** | **5.5× faster** (15.95ms) | **1.3× faster** (3.78ms) | **17.8× faster** (51.86ms) |
| branch | 7.0 ms | **2.89 ms** | **4.7× faster** (13.56ms) | **4.6× faster** (13.31ms) | **10.3× faster** (29.65ms) |
| general | 6.3 ms | **13.04 ms** | 1.1× slower (11.68ms) | tied (12.65ms) | **2.0× faster** (25.66ms) |
| hft | 7.0 ms | **153.93 ms** | 1.6× slower (96.24ms) | 1.8× slower (87.70ms) | **1.1× faster** (172.96ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/42424114218) · Source: `efe086c2e11c`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/42424114218) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42424114218) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42424114218) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42424114218) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Sable/actions/runs/42424114218) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/42424114218) · Source: `efe086c2e11c`.
