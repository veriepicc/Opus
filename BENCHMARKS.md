# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.93 ms** | 32.13 ms | 6.31 ms | 131.78 ms | **Sable** |
| **State Accumulation (50M iter)** | **5.33 ms** | 22.37 ms | 6.34 ms | 85.09 ms | **Sable** |
| **Branch Parity (40M iter)** | **4.91 ms** | 31.81 ms | 21.37 ms | 51.88 ms | **Sable** |
| **General Logic (6M iter)** | 24.51 ms | **20.37 ms** | 20.80 ms | 49.20 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 182.42 ms | 141.72 ms | **132.15 ms** | 401.33 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **47.36 ms** | 132.32 ms | 135.55 ms | 211.80 ms | **Sable** |
| **State SIMD Vector (40M items)** | **46.28 ms** | 197.28 ms | 199.43 ms | 350.30 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **47.34 ms** | 144.38 ms | 135.31 ms | 245.70 ms | **Sable** |
| **General SIMD Vector (20M items)** | **26.34 ms** | 104.72 ms | 102.18 ms | 191.21 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **26.49 ms** | 107.53 ms | 103.75 ms | 193.06 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX2`) | GCC (`-O3 -march=native`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 20.7 ms (5.0 KiB) | 260.4 ms (105.0 KiB) | 4198.1 ms (14.5 KiB) | **16.6 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **10.0 ms (5.0 KiB)** | 122.3 ms (105.0 KiB) | 117.9 ms (14.5 KiB) | 11.7 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **10.3 ms (5.0 KiB)** | 114.8 ms (105.0 KiB) | 369.7 ms (14.5 KiB) | 12.2 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.2 ms (5.0 KiB)** | 161.4 ms (105.5 KiB) | 177.1 ms (14.5 KiB) | 12.1 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **10.8 ms (6.0 KiB)** | 160.3 ms (106.0 KiB) | 225.0 ms (15.0 KiB) | 11.8 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **10.1 ms (9.5 KiB)** | 200.3 ms (106.0 KiB) | 298.2 ms (15.0 KiB) | 12.5 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **10.1 ms (8.0 KiB)** | 126.9 ms (106.0 KiB) | 138.8 ms (14.5 KiB) | 12.0 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **9.9 ms (10.5 KiB)** | 141.0 ms (106.0 KiB) | 134.2 ms (15.0 KiB) | 12.3 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **10.5 ms (8.0 KiB)** | 125.8 ms (106.0 KiB) | 127.7 ms (14.5 KiB) | 11.8 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **10.2 ms (10.0 KiB)** | 123.6 ms (106.5 KiB) | 134.9 ms (15.0 KiB) | 11.8 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 21ms / **4.93ms** | 10ms / **5.33ms** | 10ms / **4.91ms** | 10ms / **24.51ms** | 11ms / **182.42ms** | 10ms / **47.36ms** | 10ms / **46.28ms** | 10ms / **47.34ms** | 11ms / **26.34ms** | 10ms / **26.49ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 15ms / **3.05ms** | 7ms / **2.97ms** | 7ms / **3.01ms** | 7ms / **21.76ms** | 8ms / **178.08ms** | 7ms / **38.67ms** | 8ms / **42.03ms** | 7ms / **42.29ms** | 7ms / **22.34ms** | 8ms / **22.67ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 169ms / **16.21ms** | 34ms / **16.31ms** | 32ms / **16.32ms** | 42ms / **32.27ms** | 35ms / **193.96ms** | 43ms / **49.94ms** | 33ms / **47.28ms** | 32ms / **55.76ms** | 33ms / **31.97ms** | 32ms / **35.05ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 31ms / **22.22ms** | 34ms / **24.81ms** | 31ms / **22.60ms** | 54ms / **41.76ms** | 32ms / **198.69ms** | 32ms / **108.03ms** | 31ms / **109.10ms** | 32ms / **108.34ms** | 35ms / **66.51ms** | 32ms / **67.55ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43507799461) · Source: `9eea6e5bad3d`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

