# Benchmark Showdown

- **Date:** 2026-11-13
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /arch:AVX2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -march=native -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.37 ms** | 31.10 ms | 5.63 ms | — | **Sable** |
| **State Accumulation (50M iter)** | **4.38 ms** | 21.64 ms | 5.71 ms | — | **Sable** |
| **Branch Parity (40M iter)** | **4.50 ms** | 41.49 ms | 20.31 ms | — | **Sable** |
| **General Logic (6M iter)** | 26.81 ms | **18.82 ms** | 19.34 ms | — | **MSVC** |
| **HFT Order Book (12M iter)** | 189.21 ms | 140.87 ms | **131.72 ms** | — | **GCC** |
| **Math SIMD Vector (40M items)** | **119.31 ms** | 140.92 ms | 140.03 ms | — | **Sable** |
| **State SIMD Vector (40M items)** | **165.61 ms** | 202.15 ms | 212.26 ms | — | **Sable** |
| **Branch SIMD Vector (40M items)** | **121.41 ms** | 150.28 ms | 137.76 ms | — | **Sable** |
| **General SIMD Vector (20M items)** | **85.36 ms** | 108.05 ms | 106.89 ms | — | **Sable** |
| **HFT SIMD Vector (20M items)** | **102.26 ms** | 113.68 ms | 107.21 ms | — | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2 /arch:AVX2`) | GCC (`-O3 -march=native`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **17.4 ms (4.5 KiB)** | 238.0 ms (105.0 KiB) | 6113.3 ms (14.5 KiB) | — |
| **State Accumulation (50M iter)** | **8.6 ms (4.5 KiB)** | 109.1 ms (105.0 KiB) | 113.2 ms (14.5 KiB) | — |
| **Branch Parity (40M iter)** | **9.1 ms (4.5 KiB)** | 110.5 ms (105.0 KiB) | 625.1 ms (14.5 KiB) | — |
| **General Logic (6M iter)** | **12.2 ms (4.5 KiB)** | 159.4 ms (105.5 KiB) | 184.4 ms (14.5 KiB) | — |
| **HFT Order Book (12M iter)** | **9.9 ms (5.5 KiB)** | 162.1 ms (106.0 KiB) | 209.6 ms (15.0 KiB) | — |
| **Math SIMD Vector (40M items)** | **9.8 ms (8.0 KiB)** | 188.5 ms (106.0 KiB) | 234.2 ms (15.0 KiB) | — |
| **State SIMD Vector (40M items)** | **10.2 ms (10.5 KiB)** | 138.9 ms (106.0 KiB) | 128.8 ms (14.5 KiB) | — |
| **Branch SIMD Vector (40M items)** | **10.2 ms (8.5 KiB)** | 139.0 ms (106.0 KiB) | 130.6 ms (15.0 KiB) | — |
| **General SIMD Vector (20M items)** | **10.3 ms (11.5 KiB)** | 128.4 ms (106.0 KiB) | 125.0 ms (14.5 KiB) | — |
| **HFT SIMD Vector (20M items)** | **9.7 ms (10.0 KiB)** | 119.1 ms (106.5 KiB) | 133.0 ms (15.0 KiB) | — |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 17ms / **4.37ms** | 9ms / **4.38ms** | 9ms / **4.50ms** | 12ms / **26.81ms** | 10ms / **189.21ms** | 10ms / **119.31ms** | 10ms / **165.61ms** | 10ms / **121.41ms** | 10ms / **85.36ms** | 10ms / **102.26ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 9ms / **3.06ms** | 7ms / **2.97ms** | 7ms / **3.01ms** | 7ms / **21.61ms** | 8ms / **177.62ms** | 7ms / **99.48ms** | 8ms / **150.33ms** | 7ms / **112.26ms** | 7ms / **77.22ms** | 7ms / **87.69ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 127ms / **15.34ms** | 31ms / **15.58ms** | 31ms / **16.02ms** | 40ms / **30.94ms** | 34ms / **192.67ms** | 38ms / **118.54ms** | 30ms / **110.44ms** | 30ms / **140.82ms** | 30ms / **85.63ms** | 30ms / **103.64ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 32ms / **19.91ms** | 30ms / **20.22ms** | 28ms / **20.36ms** | 27ms / **39.73ms** | 30ms / **206.45ms** | 30ms / **153.16ms** | 29ms / **187.92ms** | 30ms / **158.93ms** | 26ms / **104.90ms** | 30ms / **128.76ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/43498164967) · Source: `9c92b6d6de5d`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

