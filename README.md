# Opus CI Results

## Performance

**4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 3460.6 ms | **1400.78 ms** | — | 1.0× slower (1389.53ms) | **1.0× faster** (1402.29ms) |
| state | 1557.2 ms | **1384.31 ms** | — | **1.0× faster** (1394.55ms) | **1.0× faster** (1406.17ms) |
| branch | 1401.3 ms | **1383.05 ms** | — | **1.0× faster** (1399.82ms) | **1.0× faster** (1400.37ms) |
| general | 1417.5 ms | **1399.25 ms** | — | 1.0× slower (1389.46ms) | 1.0× slower (1387.12ms) |
| hft | 1390.2 ms | **1394.87 ms** | — | **1.0× faster** (1397.87ms) | 1.0× slower (1393.87ms) |

Linux x86_64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42391885141) · Source: `6b6cd6aa8f24`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42391885141) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42391885141) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42391885141) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42391885141) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42391885141) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42391885141) · Source: `6b6cd6aa8f24`.
