# Benchmark Showdown

- **Date:** 2026-11-14
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V74 80-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX512 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -mprefer-vector-width=512 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **3.49 ms** | 27.53 ms | 4.64 ms | 115.19 ms | **Sable** |
| **State Accumulation (50M iter)** | **3.51 ms** | 18.89 ms | 4.79 ms | 74.01 ms | **Sable** |
| **Branch Parity (40M iter)** | **3.58 ms** | 27.69 ms | 17.72 ms | 40.53 ms | **Sable** |
| **General Logic (6M iter)** | 20.89 ms | **16.55 ms** | 17.18 ms | 39.07 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 148.52 ms | 120.22 ms | **117.26 ms** | 258.61 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **26.42 ms** | 121.94 ms | 121.74 ms | 185.44 ms | **Sable** |
| **State SIMD Vector (40M items)** | **26.63 ms** | 181.09 ms | 189.35 ms | 326.05 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **27.26 ms** | 122.89 ms | 124.33 ms | 214.17 ms | **Sable** |
| **General SIMD Vector (20M items)** | **15.06 ms** | 93.47 ms | 97.07 ms | 174.88 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **15.42 ms** | 96.68 ms | 100.29 ms | 177.19 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX512`) | GCC (`-O3 -march=native -mprefer-vector-width=512`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 16.5 ms (5.0 KiB) | 263.6 ms (105.0 KiB) | 4885.1 ms (14.5 KiB) | **13.2 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **8.1 ms (5.0 KiB)** | 110.6 ms (105.0 KiB) | 91.6 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **8.0 ms (5.0 KiB)** | 94.1 ms (105.0 KiB) | 406.8 ms (14.5 KiB) | 10.6 ms (2.5 KiB) |
| **General Logic (6M iter)** | **8.6 ms (5.0 KiB)** | 135.2 ms (105.5 KiB) | 158.5 ms (14.5 KiB) | 9.6 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **9.1 ms (6.0 KiB)** | 135.8 ms (106.0 KiB) | 199.4 ms (15.0 KiB) | 9.2 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **8.0 ms (9.5 KiB)** | 197.1 ms (106.5 KiB) | 257.0 ms (15.0 KiB) | 9.4 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **8.2 ms (8.0 KiB)** | 101.7 ms (106.0 KiB) | 105.0 ms (14.5 KiB) | 9.4 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **8.0 ms (10.5 KiB)** | 156.7 ms (106.5 KiB) | 103.6 ms (15.0 KiB) | 9.8 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **8.2 ms (8.0 KiB)** | 101.9 ms (106.0 KiB) | 101.8 ms (14.5 KiB) | 9.5 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **9.3 ms (10.0 KiB)** | 105.9 ms (107.0 KiB) | 107.1 ms (15.5 KiB) | 9.5 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 16ms / **3.49ms** | 8ms / **3.51ms** | 8ms / **3.58ms** | 9ms / **20.89ms** | 9ms / **148.52ms** | 8ms / **26.42ms** | 8ms / **26.63ms** | 8ms / **27.26ms** | 8ms / **15.06ms** | 9ms / **15.42ms** |
| **Windows AMD64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 15ms / **3.44ms** | 8ms / **3.48ms** | 8ms / **3.33ms** | 8ms / **23.63ms** | 8ms / **178.96ms** | 8ms / **45.19ms** | 8ms / **44.88ms** | 8ms / **43.56ms** | 8ms / **24.40ms** | 8ms / **24.32ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 123ms / **13.87ms** | 29ms / **14.27ms** | 28ms / **13.75ms** | 41ms / **29.38ms** | 32ms / **210.86ms** | 41ms / **48.73ms** | 29ms / **45.27ms** | 30ms / **54.05ms** | 29ms / **30.65ms** | 29ms / **33.81ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 31ms / **22.28ms** | 30ms / **22.44ms** | 112ms / **22.89ms** | 36ms / **45.56ms** | 31ms / **208.07ms** | 31ms / **109.43ms** | 35ms / **106.93ms** | 31ms / **106.85ms** | 31ms / **67.12ms** | 36ms / **66.35ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43531134665) · Source: `6609a5c208c1`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

