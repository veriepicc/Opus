# Opus CI Results

## Performance

Up to **250.1× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | MSVC runtime | GCC runtime | TCC runtime |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 16.3 ms | **3.41 ms** | 26.67 ms | 4.56 ms | 115.05 ms |
| state | 8.4 ms | **3.48 ms** | 18.26 ms | 4.43 ms | 73.73 ms |
| branch | 7.7 ms | **3.48 ms** | 27.03 ms | 17.63 ms | 39.87 ms |
| general | 8.0 ms | 16.98 ms | 16.34 ms | **15.64 ms** | 38.71 ms |
| hft | 8.2 ms | 219.56 ms | 120.39 ms | **112.40 ms** | 258.25 ms |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42388174690) · Source: `54925777f8e8`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42388174690) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388174690) |
| Windows Server 2025 | Native x64 | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388174690) |
| Windows 11 on Arm | x64 emulation | 328 | 3 | 4 | [Failed](https://github.com/veriepicc/Opus/actions/runs/42388174690) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42388174690) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42388174690) · Source: `54925777f8e8`.
