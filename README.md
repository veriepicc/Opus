# Opus CI Results

## Performance

Up to **145.3× faster compilation** than MSVC · **4.5 KiB** smallest Opus executable.

| Workload | Opus compile | Opus runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 25.6 ms | **4.61 ms** | **7.0× faster** (32.06ms) | **1.3× faster** (5.87ms) | **28.7× faster** (131.97ms) |
| state | 9.5 ms | **4.78 ms** | **4.7× faster** (22.59ms) | **1.3× faster** (6.14ms) | **17.8× faster** (85.17ms) |
| branch | 9.5 ms | **4.76 ms** | **6.7× faster** (31.75ms) | **4.5× faster** (21.47ms) | **10.9× faster** (51.78ms) |
| general | 10.2 ms | **22.38 ms** | 1.1× slower (19.64ms) | 1.2× slower (18.82ms) | **2.2× faster** (49.07ms) |
| hft | 11.2 ms | **427.01 ms** | 3.0× slower (141.63ms) | 3.2× slower (132.78ms) | 1.1× slower (400.69ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Opus/actions/runs/42399973206) · Source: `7e41415d8a96`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Opus/actions/runs/42399973206) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42399973206) |
| Windows Server 2025 | Native x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42399973206) |
| Windows 11 on Arm | x64 emulation | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42399973206) |
| Ubuntu 24.04 | Wine x64 | 331 | 0 | 4 | [Passed](https://github.com/veriepicc/Opus/actions/runs/42399973206) |

Results: [CI run](https://github.com/veriepicc/Opus/actions/runs/42399973206) · Source: `7e41415d8a96`.
