# Opus CI Results

## Performance

Up to **255.1× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | MSVC runtime | GCC runtime | TCC runtime |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 14.5 ms | **4.45 ms** | 31.04 ms | 5.59 ms | 131.31 ms |
| state | 8.9 ms | **4.38 ms** | 21.04 ms | 5.57 ms | 84.17 ms |
| branch | 9.0 ms | **4.28 ms** | 30.65 ms | 20.40 ms | 50.70 ms |
| general | 9.1 ms | 21.57 ms | 18.54 ms | **17.82 ms** | 47.96 ms |
| hft | 11.5 ms | 428.34 ms | 140.90 ms | **132.20 ms** | 400.68 ms |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42389890154) · Source: `801e952314db`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42389890154) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42389890154) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42389890154) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42389890154) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42389890154) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42389890154) · Source: `801e952314db`.
