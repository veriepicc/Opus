# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX512 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -mprefer-vector-width=512 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **2.86 ms** | 22.33 ms | 3.71 ms | 79.37 ms | **Sable** |
| **State Accumulation (50M iter)** | **2.79 ms** | 15.33 ms | 3.70 ms | 50.58 ms | **Sable** |
| **Branch Parity (40M iter)** | **3.00 ms** | 14.04 ms | 13.29 ms | 29.20 ms | **Sable** |
| **General Logic (6M iter)** | 12.56 ms | **11.59 ms** | 12.75 ms | 25.12 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 112.90 ms | 94.02 ms | **88.59 ms** | 170.44 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **22.08 ms** | 102.30 ms | 102.15 ms | 153.11 ms | **Sable** |
| **State SIMD Vector (40M items)** | **22.21 ms** | 142.12 ms | 157.81 ms | 245.38 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **22.24 ms** | 103.58 ms | 103.07 ms | 173.42 ms | **Sable** |
| **General SIMD Vector (20M items)** | **12.67 ms** | 72.93 ms | 79.65 ms | 131.83 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **12.74 ms** | 78.53 ms | 78.90 ms | 132.92 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX512`) | GCC (`-O3 -march=native -mprefer-vector-width=512`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 16.5 ms (5.0 KiB) | 225.6 ms (105.0 KiB) | 3979.0 ms (14.5 KiB) | **9.8 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **5.8 ms (5.0 KiB)** | 94.2 ms (105.0 KiB) | 71.2 ms (14.5 KiB) | 7.1 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **5.7 ms (5.0 KiB)** | 74.0 ms (105.0 KiB) | 323.5 ms (14.5 KiB) | 7.2 ms (2.5 KiB) |
| **General Logic (6M iter)** | **6.0 ms (5.0 KiB)** | 98.2 ms (105.5 KiB) | 123.5 ms (14.5 KiB) | 7.1 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **6.5 ms (6.0 KiB)** | 116.2 ms (106.0 KiB) | 172.2 ms (15.0 KiB) | 8.0 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **6.2 ms (9.5 KiB)** | 167.1 ms (106.5 KiB) | 221.7 ms (15.0 KiB) | 8.7 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **7.5 ms (8.0 KiB)** | 84.8 ms (106.0 KiB) | 82.6 ms (14.5 KiB) | 8.5 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **7.1 ms (10.5 KiB)** | 94.8 ms (106.5 KiB) | 83.5 ms (15.0 KiB) | 8.8 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **7.4 ms (8.0 KiB)** | 81.7 ms (106.0 KiB) | 78.3 ms (14.5 KiB) | 8.6 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **7.4 ms (10.0 KiB)** | 85.2 ms (107.0 KiB) | 84.3 ms (15.5 KiB) | 8.3 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 16ms / **2.86ms** | 6ms / **2.79ms** | 6ms / **3.00ms** | 6ms / **12.56ms** | 6ms / **112.90ms** | 6ms / **22.08ms** | 7ms / **22.21ms** | 7ms / **22.24ms** | 7ms / **12.67ms** | 7ms / **12.74ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 15ms / **3.28ms** | 7ms / **3.06ms** | 7ms / **3.13ms** | 8ms / **22.06ms** | 8ms / **187.22ms** | 8ms / **43.69ms** | 8ms / **44.54ms** | 7ms / **44.74ms** | 8ms / **24.28ms** | 7ms / **24.69ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 122ms / **15.85ms** | 30ms / **15.50ms** | 30ms / **15.81ms** | 41ms / **31.77ms** | 34ms / **211.50ms** | 44ms / **50.52ms** | 31ms / **47.26ms** | 32ms / **55.23ms** | 33ms / **31.76ms** | 33ms / **35.09ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) 6975P-C` | 21ms / **7.74ms** | 22ms / **10.70ms** | 12ms / **7.47ms** | 21ms / **18.86ms** | 13ms / **139.15ms** | 28ms / **77.67ms** | 22ms / **71.96ms** | 23ms / **71.58ms** | 23ms / **47.40ms** | 13ms / **45.55ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43527526701) · Source: `86b5c0bc5572`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

