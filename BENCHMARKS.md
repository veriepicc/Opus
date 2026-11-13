# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **5.03 ms** | 32.37 ms | 6.73 ms | 131.89 ms | **Sable** |
| **State Accumulation (50M iter)** | **4.97 ms** | 22.95 ms | 6.74 ms | 85.79 ms | **Sable** |
| **Branch Parity (40M iter)** | **5.08 ms** | 32.47 ms | 21.53 ms | 51.98 ms | **Sable** |
| **General Logic (6M iter)** | 24.83 ms | **20.16 ms** | 20.97 ms | 49.50 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 192.65 ms | 141.90 ms | **132.10 ms** | 401.68 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **50.63 ms** | 139.72 ms | 141.97 ms | 211.20 ms | **Sable** |
| **State SIMD Vector (40M items)** | **52.61 ms** | 187.96 ms | 212.91 ms | 361.03 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **52.93 ms** | 140.93 ms | 137.82 ms | 251.89 ms | **Sable** |
| **General SIMD Vector (20M items)** | **29.41 ms** | 111.50 ms | 110.50 ms | 195.65 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **28.51 ms** | 115.62 ms | 109.92 ms | 197.22 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX2`) | GCC (`-O3 -march=native`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 22.5 ms (5.0 KiB) | 278.1 ms (105.0 KiB) | 9089.7 ms (14.5 KiB) | **18.0 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **10.0 ms (5.0 KiB)** | 125.5 ms (105.0 KiB) | 119.8 ms (14.5 KiB) | 12.4 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **9.9 ms (5.0 KiB)** | 119.6 ms (105.0 KiB) | 392.9 ms (14.5 KiB) | 12.5 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.7 ms (5.0 KiB)** | 151.9 ms (105.5 KiB) | 179.4 ms (14.5 KiB) | 12.6 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **11.1 ms (6.0 KiB)** | 171.7 ms (106.0 KiB) | 237.4 ms (15.0 KiB) | 12.1 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | **11.2 ms (9.5 KiB)** | 241.4 ms (106.0 KiB) | 271.4 ms (15.0 KiB) | 13.1 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **10.5 ms (8.0 KiB)** | 146.8 ms (106.0 KiB) | 144.3 ms (14.5 KiB) | 12.8 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **11.3 ms (10.5 KiB)** | 166.7 ms (106.0 KiB) | 146.3 ms (15.0 KiB) | 13.1 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **10.9 ms (8.0 KiB)** | 129.2 ms (106.0 KiB) | 133.6 ms (14.5 KiB) | 13.9 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **11.3 ms (10.0 KiB)** | 141.3 ms (106.5 KiB) | 138.1 ms (15.0 KiB) | 12.6 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 22ms / **5.03ms** | 10ms / **4.97ms** | 10ms / **5.08ms** | 11ms / **24.83ms** | 11ms / **192.65ms** | 11ms / **50.63ms** | 10ms / **52.61ms** | 11ms / **52.93ms** | 11ms / **29.41ms** | 11ms / **28.51ms** |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 17ms / **2.52ms** | 6ms / **2.53ms** | 6ms / **2.56ms** | 7ms / **19.62ms** | 7ms / **145.70ms** | 6ms / **25.11ms** | 6ms / **24.17ms** | 6ms / **25.81ms** | 6ms / **14.72ms** | 7ms / **14.91ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 294ms / **14.92ms** | 32ms / **15.23ms** | 36ms / **14.69ms** | 40ms / **31.48ms** | 34ms / **211.88ms** | 41ms / **48.68ms** | 30ms / **42.82ms** | 31ms / **54.81ms** | 30ms / **30.55ms** | 34ms / **34.51ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) 6975P-C` | 14ms / **7.62ms** | 22ms / **10.40ms** | 22ms / **10.46ms** | 23ms / **19.76ms** | 24ms / **141.79ms** | 13ms / **77.43ms** | 25ms / **75.98ms** | 14ms / **74.28ms** | 24ms / **42.93ms** | 23ms / **42.67ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43529715163) · Source: `da659d44abe3`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

