# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V74 80-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX512 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -mprefer-vector-width=512 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **3.92 ms** | 26.69 ms | 4.87 ms | 115.07 ms | **Sable** |
| **State Accumulation (50M iter)** | **3.63 ms** | 19.13 ms | 4.68 ms | 73.72 ms | **Sable** |
| **Branch Parity (40M iter)** | **3.62 ms** | 27.60 ms | 18.11 ms | 40.17 ms | **Sable** |
| **General Logic (6M iter)** | 21.05 ms | **16.96 ms** | 17.32 ms | 38.82 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 143.13 ms | 120.30 ms | **117.40 ms** | 258.14 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **27.37 ms** | 122.02 ms | 123.43 ms | 184.17 ms | **Sable** |
| **State SIMD Vector (40M items)** | **27.90 ms** | 177.38 ms | 186.72 ms | 321.52 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **27.00 ms** | 124.17 ms | 124.42 ms | 210.13 ms | **Sable** |
| **General SIMD Vector (20M items)** | **14.79 ms** | 93.15 ms | 95.83 ms | 172.56 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **15.48 ms** | 96.08 ms | 96.13 ms | 173.40 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX512`) | GCC (`-O3 -march=native -mprefer-vector-width=512`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 16.2 ms (5.0 KiB) | 230.0 ms (105.0 KiB) | 3454.7 ms (14.5 KiB) | **12.5 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **8.7 ms (5.0 KiB)** | 98.1 ms (105.0 KiB) | 94.3 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **7.9 ms (5.0 KiB)** | 89.8 ms (105.0 KiB) | 376.5 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **General Logic (6M iter)** | **8.1 ms (5.0 KiB)** | 114.6 ms (105.5 KiB) | 146.4 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **8.9 ms (6.0 KiB)** | 132.8 ms (106.0 KiB) | 207.3 ms (15.0 KiB) | 9.2 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **8.2 ms (9.5 KiB)** | 194.8 ms (106.5 KiB) | 233.4 ms (15.0 KiB) | 9.6 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **8.5 ms (8.0 KiB)** | 104.9 ms (106.0 KiB) | 109.2 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **8.1 ms (10.5 KiB)** | 113.4 ms (106.5 KiB) | 102.9 ms (15.0 KiB) | 9.7 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **8.0 ms (8.0 KiB)** | 99.9 ms (106.0 KiB) | 98.6 ms (14.5 KiB) | 9.5 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **8.0 ms (10.0 KiB)** | 111.2 ms (107.0 KiB) | 110.2 ms (15.5 KiB) | 10.6 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Measurement Errors

- hft: toolchain result checksums disagree

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 16ms / **3.92ms** | 9ms / **3.63ms** | 8ms / **3.62ms** | 8ms / **21.05ms** | 9ms / **143.13ms** | 8ms / **27.37ms** | 9ms / **27.90ms** | 8ms / **27.00ms** | 8ms / **14.79ms** | 8ms / **15.48ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 20ms / **3.05ms** | 7ms / **3.02ms** | 7ms / **3.03ms** | 7ms / **21.67ms** | 8ms / **180.89ms** | 7ms / **42.60ms** | 8ms / **42.18ms** | 8ms / **42.69ms** | 7ms / **22.44ms** | 7ms / **22.46ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 200` | 109ms / **10.44ms** | 21ms / **10.52ms** | 20ms / **10.68ms** | 28ms / **21.53ms** | 23ms / **642.49ms** | 31ms / **41.60ms** | 21ms / **39.01ms** | 22ms / **48.52ms** | 21ms / **25.61ms** | 22ms / **28.25ms** |
| **Linux x86_64** | `AMD EPYC 9V74 80-Core Processor` | 25ms / **17.73ms** | 27ms / **17.81ms** | 25ms / **18.60ms** | 25ms / **34.30ms** | 76ms / **158.65ms** | 25ms / **78.23ms** | 28ms / **76.69ms** | 26ms / **77.19ms** | 26ms / **48.00ms** | 25ms / **49.40ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43523484571) · Source: `673aff00bd5d`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

