# Sable CI Results

## Performance

Up to **26.9× faster compilation** than MSVC · **5.0 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 16.5 ms | **2.86 ms** | **7.8× faster** (22.33ms) | **1.3× faster** (3.71ms) | **27.7× faster** (79.37ms) |
| state | 5.8 ms | **2.79 ms** | **5.5× faster** (15.33ms) | **1.3× faster** (3.70ms) | **18.1× faster** (50.58ms) |
| branch | 5.7 ms | **3.00 ms** | **4.7× faster** (14.04ms) | **4.4× faster** (13.29ms) | **9.7× faster** (29.20ms) |
| general | 6.0 ms | **12.56 ms** | 1.1× slower (11.59ms) | tied (12.75ms) | **2.0× faster** (25.12ms) |
| hft | 6.5 ms | **112.90 ms** | 1.2× slower (94.02ms) | 1.3× slower (88.59ms) | **1.5× faster** (170.44ms) |
| math_vec | 6.2 ms | **22.08 ms** | **4.6× faster** (102.30ms) | **4.6× faster** (102.15ms) | **6.9× faster** (153.11ms) |
| state_vec | 7.5 ms | **22.21 ms** | **6.4× faster** (142.12ms) | **7.1× faster** (157.81ms) | **11.0× faster** (245.38ms) |
| branch_vec | 7.1 ms | **22.24 ms** | **4.7× faster** (103.58ms) | **4.6× faster** (103.07ms) | **7.8× faster** (173.42ms) |
| general_vec | 7.4 ms | **12.67 ms** | **5.8× faster** (72.93ms) | **6.3× faster** (79.65ms) | **10.4× faster** (131.83ms) |
| hft_vec | 7.4 ms | **12.74 ms** | **6.2× faster** (78.53ms) | **6.2× faster** (78.90ms) | **10.4× faster** (132.92ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43527526701) · Source: `86b5c0bc5572`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43527526701) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43527526701) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43527526701) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43527526701) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43527526701) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43527526701) · Source: `86b5c0bc5572`.
