# Sable CI Results

## Performance

Up to **19.9× faster compilation** than MSVC · **5.0 KiB** smallest Sable executable.

| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |
| :--- | ---: | ---: | ---: | ---: | ---: |
| math | 20.7 ms | **4.93 ms** | **6.5× faster** (32.13ms) | **1.3× faster** (6.31ms) | **26.7× faster** (131.78ms) |
| state | 10.0 ms | **5.33 ms** | **4.2× faster** (22.37ms) | **1.2× faster** (6.34ms) | **16.0× faster** (85.09ms) |
| branch | 10.3 ms | **4.91 ms** | **6.5× faster** (31.81ms) | **4.4× faster** (21.37ms) | **10.6× faster** (51.88ms) |
| general | 10.2 ms | **24.51 ms** | 1.2× slower (20.37ms) | 1.2× slower (20.80ms) | **2.0× faster** (49.20ms) |
| hft | 10.8 ms | **182.42 ms** | 1.3× slower (141.72ms) | 1.4× slower (132.15ms) | **2.2× faster** (401.33ms) |
| math_vec | 10.1 ms | **47.36 ms** | **2.8× faster** (132.32ms) | **2.9× faster** (135.55ms) | **4.5× faster** (211.80ms) |
| state_vec | 10.1 ms | **46.28 ms** | **4.3× faster** (197.28ms) | **4.3× faster** (199.43ms) | **7.6× faster** (350.30ms) |
| branch_vec | 9.9 ms | **47.34 ms** | **3.0× faster** (144.38ms) | **2.9× faster** (135.31ms) | **5.2× faster** (245.70ms) |
| general_vec | 10.5 ms | **26.34 ms** | **4.0× faster** (104.72ms) | **3.9× faster** (102.18ms) | **7.3× faster** (191.21ms) |
| hft_vec | 10.2 ms | **26.49 ms** | **4.1× faster** (107.53ms) | **3.9× faster** (103.75ms) | **7.3× faster** (193.06ms) |

Windows AMD64 · 5 measured runs · 2 warmups · minimum runtime.
[Full results & methodology](BENCHMARKS.md)

[Benchmark CI run](https://github.com/veriepicc/Sable/actions/runs/43507799461) · Source: `9eea6e5bad3d`.


## Core CI Build Status

| Host & Toolchain | Target | Configuration | Build |
| :--- | :---: | :---: | :---: |
| Windows Server 2022 · MSVC | x64 | Release | [Passed](https://github.com/veriepicc/Sable/actions/runs/43507799461) |

## Platform CI Test Status

| Test Host | Execution | Passed | Failed | Skipped | Result |
| :--- | :--- | ---: | ---: | ---: | :---: |
| Windows Server 2022 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43507799461) |
| Windows Server 2025 | Native x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43507799461) |
| Windows 11 on Arm | x64 emulation | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43507799461) |
| Ubuntu 24.04 | Wine x64 | — | — | — | [No report](https://github.com/veriepicc/Sable/actions/runs/43507799461) |

Results: [CI run](https://github.com/veriepicc/Sable/actions/runs/43507799461) · Source: `9eea6e5bad3d`.
