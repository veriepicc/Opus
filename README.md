# Opus CI Results

## Performance

Up to **293.8× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 12.5 ms | **3.34 ms** | **7.9× faster** (26.36ms) | **1.3× faster** (4.26ms) | **34.4× faster** (114.77ms) |
| state | 7.2 ms | **3.38 ms** | **5.4× faster** (18.22ms) | **1.3× faster** (4.39ms) | **21.7× faster** (73.34ms) |
| branch | 7.2 ms | **3.34 ms** | **8.0× faster** (26.60ms) | **5.2× faster** (17.46ms) | **11.8× faster** (39.44ms) |
| general | 7.6 ms | **16.74 ms** | tied (15.96ms) | 1.1× slower (15.48ms) | **2.3× faster** (38.03ms) |
| hft | 7.9 ms | **219.36 ms** | 1.8× slower (120.02ms) | 2.0× slower (112.01ms) | **1.2× faster** (258.53ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42397081142) · Source: `9fb0749b24a1`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397081142) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397081142) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397081142) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397081142) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42397081142) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42397081142) · Source: `9fb0749b24a1`.
