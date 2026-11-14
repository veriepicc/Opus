# Benchmark Showdown

- **Date:** 2026-11-14
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX512 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -mprefer-vector-width=512 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **2.86 ms** | 22.45 ms | 3.59 ms | 76.28 ms | **Sable** |
| **State Accumulation (50M iter)** | **2.82 ms** | 15.14 ms | 3.64 ms | 49.21 ms | **Sable** |
| **Branch Parity (40M iter)** | **2.85 ms** | 14.38 ms | 13.15 ms | 28.36 ms | **Sable** |
| **General Logic (6M iter)** | 13.13 ms | **11.03 ms** | 13.45 ms | 25.51 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 107.78 ms | 90.35 ms | **84.55 ms** | 162.50 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **21.98 ms** | 98.90 ms | 99.07 ms | 144.32 ms | **Sable** |
| **State SIMD Vector (40M items)** | **22.75 ms** | 136.73 ms | 152.44 ms | 232.09 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **23.29 ms** | 99.05 ms | 99.87 ms | 159.85 ms | **Sable** |
| **General SIMD Vector (20M items)** | **12.59 ms** | 71.89 ms | 77.07 ms | 125.14 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **13.58 ms** | 77.53 ms | 78.73 ms | 128.03 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX512`) | GCC (`-O3 -march=native -mprefer-vector-width=512`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **9.2 ms (5.0 KiB)** | 277.2 ms (105.0 KiB) | 4637.9 ms (14.5 KiB) | 10.8 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **7.1 ms (5.0 KiB)** | 83.9 ms (105.0 KiB) | 77.2 ms (14.5 KiB) | 7.8 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **6.9 ms (5.0 KiB)** | 79.6 ms (105.0 KiB) | 410.1 ms (14.5 KiB) | 8.2 ms (2.5 KiB) |
| **General Logic (6M iter)** | 7.4 ms (5.0 KiB) | 102.4 ms (105.5 KiB) | 146.5 ms (14.5 KiB) | **7.3 ms (2.5 KiB)** |
| **HFT Order Book (12M iter)** | **7.8 ms (6.0 KiB)** | 131.3 ms (106.0 KiB) | 187.9 ms (15.0 KiB) | 8.3 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **7.0 ms (9.5 KiB)** | 171.0 ms (106.5 KiB) | 263.8 ms (15.0 KiB) | 8.0 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **7.1 ms (8.0 KiB)** | 91.6 ms (106.0 KiB) | 84.5 ms (14.5 KiB) | 8.3 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **7.1 ms (10.5 KiB)** | 92.3 ms (106.5 KiB) | 83.5 ms (15.0 KiB) | 8.0 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **7.0 ms (8.0 KiB)** | 82.9 ms (106.0 KiB) | 77.6 ms (14.5 KiB) | 9.1 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **7.1 ms (10.0 KiB)** | 110.2 ms (107.0 KiB) | 91.9 ms (15.5 KiB) | 8.5 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 9ms / **2.86ms** | 7ms / **2.82ms** | 7ms / **2.85ms** | 7ms / **13.13ms** | 8ms / **107.78ms** | 7ms / **21.98ms** | 7ms / **22.75ms** | 7ms / **23.29ms** | 7ms / **12.59ms** | 7ms / **13.58ms** |
| **Windows AMD64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 10ms / **3.30ms** | 7ms / **3.09ms** | 7ms / **3.13ms** | 8ms / **21.18ms** | 8ms / **196.09ms** | 11ms / **40.96ms** | 8ms / **42.21ms** | 8ms / **42.80ms** | 10ms / **23.54ms** | 8ms / **23.06ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 125ms / **13.29ms** | 28ms / **13.42ms** | 28ms / **13.68ms** | 36ms / **29.06ms** | 31ms / **210.45ms** | 39ms / **47.54ms** | 27ms / **44.17ms** | 28ms / **52.55ms** | 28ms / **29.71ms** | 28ms / **32.88ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 30ms / **22.27ms** | 34ms / **21.92ms** | 33ms / **21.84ms** | 31ms / **41.00ms** | 36ms / **206.95ms** | 33ms / **109.25ms** | 38ms / **106.56ms** | 31ms / **108.77ms** | 36ms / **65.17ms** | 31ms / **65.68ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43531408198) · Source: `ea7cdc96a6e5`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

