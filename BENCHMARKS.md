# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.46 ms** | 31.37 ms | 5.72 ms | 131.29 ms | **Sable** |
| **State Accumulation (50M iter)** | **4.30 ms** | 22.22 ms | 5.53 ms | 84.58 ms | **Sable** |
| **Branch Parity (40M iter)** | **4.43 ms** | 31.57 ms | 20.46 ms | 51.53 ms | **Sable** |
| **General Logic (6M iter)** | 24.12 ms | **18.87 ms** | 19.54 ms | 48.84 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 182.36 ms | 141.39 ms | **131.58 ms** | 400.65 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **112.59 ms** | 132.57 ms | 133.76 ms | 209.48 ms | **Sable** |
| **State SIMD Vector (40M items)** | **153.25 ms** | 191.57 ms | 199.35 ms | 349.83 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **120.56 ms** | 142.90 ms | 134.48 ms | 241.80 ms | **Sable** |
| **General SIMD Vector (20M items)** | **81.23 ms** | 104.51 ms | 100.37 ms | 191.19 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **95.69 ms** | 108.31 ms | 101.85 ms | 193.80 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX2`) | GCC (`-O3 -march=native`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **13.1 ms (4.5 KiB)** | 230.8 ms (105.0 KiB) | 3385.7 ms (14.5 KiB) | 15.8 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **9.4 ms (4.5 KiB)** | 106.7 ms (105.0 KiB) | 112.7 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **9.3 ms (4.5 KiB)** | 108.1 ms (105.0 KiB) | 338.2 ms (14.5 KiB) | 11.4 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.0 ms (4.5 KiB)** | 128.9 ms (105.5 KiB) | 162.4 ms (14.5 KiB) | 12.7 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **11.0 ms (5.5 KiB)** | 149.1 ms (106.0 KiB) | 211.2 ms (15.0 KiB) | 11.7 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **9.5 ms (8.0 KiB)** | 184.4 ms (106.0 KiB) | 250.1 ms (15.0 KiB) | 11.4 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **9.4 ms (10.5 KiB)** | 126.1 ms (106.0 KiB) | 130.0 ms (14.5 KiB) | 10.9 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **11.5 ms (8.5 KiB)** | 134.5 ms (106.0 KiB) | 144.8 ms (15.0 KiB) | 12.2 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **9.8 ms (11.5 KiB)** | 110.6 ms (106.0 KiB) | 120.9 ms (14.5 KiB) | 11.2 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **10.2 ms (10.0 KiB)** | 121.6 ms (106.5 KiB) | 128.7 ms (15.0 KiB) | 11.1 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 13ms / **4.46ms** | 9ms / **4.30ms** | 9ms / **4.43ms** | 10ms / **24.12ms** | 11ms / **182.36ms** | 9ms / **112.59ms** | 9ms / **153.25ms** | 12ms / **120.56ms** | 10ms / **81.23ms** | 10ms / **95.69ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 12ms / **3.14ms** | 7ms / **3.20ms** | 7ms / **3.13ms** | 8ms / **22.04ms** | 9ms / **177.99ms** | 7ms / **104.45ms** | 8ms / **145.39ms** | 7ms / **110.66ms** | 7ms / **80.22ms** | 7ms / **92.73ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 140ms / **13.45ms** | 30ms / **14.55ms** | 28ms / **13.71ms** | 38ms / **29.74ms** | 32ms / **192.67ms** | 37ms / **116.75ms** | 30ms / **107.98ms** | 29ms / **143.07ms** | 29ms / **82.35ms** | 29ms / **104.16ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) 6975P-C` | 22ms / **7.56ms** | 11ms / **8.18ms** | 22ms / **7.11ms** | 19ms / **25.25ms** | 20ms / **134.09ms** | 12ms / **108.91ms** | 23ms / **138.59ms** | 23ms / **108.61ms** | 21ms / **73.79ms** | 13ms / **93.65ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43499690339) · Source: `9d28159c90dd`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

