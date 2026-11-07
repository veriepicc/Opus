# Opus CI Results

## Performance

Up to **180.7× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | MSVC runtime | GCC runtime | TCC runtime |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 22.1 ms | **4.45 ms** | 30.82 ms | 5.60 ms | 131.90 ms |
| state | 10.1 ms | **4.32 ms** | 21.07 ms | 5.58 ms | 83.81 ms |
| branch | 9.6 ms | **4.64 ms** | 30.77 ms | 20.32 ms | 51.29 ms |
| general | 9.5 ms | 21.39 ms | 18.91 ms | **18.03 ms** | 48.01 ms |
| hft | 9.7 ms | 427.19 ms | 141.27 ms | **132.22 ms** | 402.91 ms |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42388809208) · Source: `996dffc19328`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42388809208) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388809208) |
| Windows Server 2025 | Native x64 | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388809208) |
| Windows 11 on Arm | x64 emulation | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388809208) |
| Ubuntu 24.04 | Wine x64 | 330 | 1 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388809208) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42388809208) · Source: `996dffc19328`.
