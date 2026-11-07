# Opus CI Results

## Performance

Up to **170.3× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 24.0 ms | **3.00 ms** | **7.1× faster** (21.22ms) | **1.2× faster** (3.74ms) | **25.0× faster** (75.08ms) |
| state | 6.7 ms | **2.69 ms** | **5.8× faster** (15.70ms) | **1.4× faster** (3.66ms) | **17.9× faster** (47.95ms) |
| branch | 5.6 ms | **2.62 ms** | **5.3× faster** (13.96ms) | **5.2× faster** (13.58ms) | **11.3× faster** (29.76ms) |
| general | 6.9 ms | **12.37 ms** | 1.1× slower (11.10ms) | tied (12.76ms) | **2.0× faster** (25.26ms) |
| hft | 7.6 ms | **144.88 ms** | 1.6× slower (89.61ms) | 1.8× slower (82.54ms) | **1.1× faster** (162.70ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42402978739) · Source: `e734f72ccb2e`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402978739) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402978739) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402978739) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402978739) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42402978739) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42402978739) · Source: `e734f72ccb2e`.
