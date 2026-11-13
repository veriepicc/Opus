# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.35 ms** | 30.37 ms | 5.56 ms | 130.94 ms | **Sable** |
| **State Accumulation (50M iter)** | **4.31 ms** | 21.23 ms | 5.52 ms | 83.89 ms | **Sable** |
| **Branch Parity (40M iter)** | **4.58 ms** | 30.56 ms | 19.87 ms | 50.77 ms | **Sable** |
| **General Logic (6M iter)** | 23.34 ms | **18.92 ms** | 20.73 ms | 48.71 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 182.06 ms | 141.46 ms | **131.66 ms** | 401.56 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **115.98 ms** | 134.76 ms | 139.69 ms | 213.70 ms | **Sable** |
| **State SIMD Vector (40M items)** | **156.32 ms** | 192.21 ms | 199.91 ms | 354.35 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **118.74 ms** | 146.72 ms | 133.67 ms | 243.28 ms | **Sable** |
| **General SIMD Vector (20M items)** | **80.35 ms** | 104.67 ms | 103.29 ms | 191.94 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **97.86 ms** | 107.35 ms | 103.52 ms | 194.42 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX2`) | GCC (`-O3 -march=native`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 18.8 ms (4.5 KiB) | 271.7 ms (105.0 KiB) | 3752.2 ms (14.5 KiB) | **15.0 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **8.6 ms (4.5 KiB)** | 104.4 ms (105.0 KiB) | 106.9 ms (14.5 KiB) | 10.8 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | 10.5 ms (4.5 KiB) | 104.4 ms (105.0 KiB) | 353.5 ms (14.5 KiB) | **10.3 ms (2.5 KiB)** |
| **General Logic (6M iter)** | **9.5 ms (4.5 KiB)** | 140.2 ms (105.5 KiB) | 186.3 ms (14.5 KiB) | 12.3 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **10.1 ms (5.5 KiB)** | 156.0 ms (106.0 KiB) | 217.0 ms (15.0 KiB) | 11.3 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **10.8 ms (8.0 KiB)** | 200.2 ms (106.0 KiB) | 245.3 ms (15.0 KiB) | 11.7 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **10.5 ms (10.5 KiB)** | 117.4 ms (106.0 KiB) | 126.7 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **9.2 ms (8.5 KiB)** | 134.3 ms (106.0 KiB) | 128.8 ms (15.0 KiB) | 12.3 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **9.5 ms (11.5 KiB)** | 113.7 ms (106.0 KiB) | 118.6 ms (14.5 KiB) | 11.2 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **9.4 ms (10.0 KiB)** | 118.2 ms (106.5 KiB) | 124.5 ms (15.0 KiB) | 11.5 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 19ms / **4.35ms** | 9ms / **4.31ms** | 10ms / **4.58ms** | 9ms / **23.34ms** | 10ms / **182.06ms** | 11ms / **115.98ms** | 10ms / **156.32ms** | 9ms / **118.74ms** | 10ms / **80.35ms** | 9ms / **97.86ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 16ms / **3.33ms** | 7ms / **3.05ms** | 7ms / **3.03ms** | 8ms / **21.82ms** | 8ms / **178.37ms** | 8ms / **105.32ms** | 8ms / **146.02ms** | 7ms / **107.28ms** | 8ms / **79.52ms** | 8ms / **92.71ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 139ms / **16.25ms** | 33ms / **16.55ms** | 32ms / **16.35ms** | 42ms / **32.17ms** | 36ms / **193.62ms** | 41ms / **118.80ms** | 32ms / **115.04ms** | 30ms / **143.47ms** | 31ms / **85.05ms** | 33ms / **105.03ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 31ms / **18.76ms** | 30ms / **20.02ms** | 78ms / **20.61ms** | 32ms / **39.41ms** | 30ms / **209.28ms** | 30ms / **157.72ms** | 38ms / **194.12ms** | 30ms / **165.29ms** | 27ms / **104.89ms** | 29ms / **131.89ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43500729658) · Source: `7a39b9eab9c9`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

