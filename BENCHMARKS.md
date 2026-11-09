# Benchmark Showdown

- **Date:** 2026-11-09
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V74 80-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | 33.98 ms | 35.13 ms | **5.73 ms** | 148.06 ms | **GCC** |
| **State Accumulation (50M iter)** | 41.03 ms | 24.26 ms | **5.87 ms** | 94.73 ms | **GCC** |
| **Branch Parity (40M iter)** | 48.15 ms | 35.14 ms | **22.74 ms** | 51.79 ms | **GCC** |
| **General Logic (6M iter)** | 22.77 ms | 21.63 ms | **20.94 ms** | 49.72 ms | **GCC** |
| **HFT Order Book (12M iter)** | 282.79 ms | 154.87 ms | **144.58 ms** | 333.68 ms | **GCC** |
| **Math SIMD Vector (40M items)** | — | **143.62 ms** | 147.91 ms | 225.99 ms | **MSVC** |
| **State SIMD Vector (40M items)** | — | **210.41 ms** | 217.14 ms | 399.70 ms | **MSVC** |
| **Branch SIMD Vector (40M items)** | — | 152.63 ms | **143.78 ms** | 259.12 ms | **GCC** |
| **General SIMD Vector (20M items)** | — | 112.13 ms | **111.31 ms** | 215.24 ms | **GCC** |
| **HFT SIMD Vector (20M items)** | — | 115.33 ms | **112.79 ms** | 216.68 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **15.6 ms (4.5 KiB)** | 4895.9 ms (105.0 KiB) | 4326.5 ms (14.5 KiB) | 17.7 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **9.4 ms (4.5 KiB)** | 120.0 ms (105.0 KiB) | 113.4 ms (14.5 KiB) | 11.2 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **10.1 ms (4.5 KiB)** | 116.0 ms (105.0 KiB) | 392.4 ms (14.5 KiB) | 12.1 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.4 ms (4.5 KiB)** | 143.1 ms (105.5 KiB) | 183.9 ms (14.5 KiB) | 12.3 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **11.2 ms (7.0 KiB)** | 191.0 ms (106.0 KiB) | 213.8 ms (15.0 KiB) | 11.8 ms (4.0 KiB) |
| **Math SIMD Vector (40M items)** | — | 237.4 ms (106.0 KiB) | 267.5 ms (15.0 KiB) | **12.6 ms (2.5 KiB)** |
| **State SIMD Vector (40M items)** | — | 127.1 ms (106.0 KiB) | 140.8 ms (14.5 KiB) | **12.3 ms (2.5 KiB)** |
| **Branch SIMD Vector (40M items)** | — | 156.2 ms (106.0 KiB) | 130.6 ms (15.0 KiB) | **11.3 ms (2.5 KiB)** |
| **General SIMD Vector (20M items)** | — | 123.0 ms (106.0 KiB) | 122.0 ms (14.5 KiB) | **11.8 ms (2.5 KiB)** |
| **HFT SIMD Vector (20M items)** | — | 126.3 ms (106.5 KiB) | 136.9 ms (15.0 KiB) | **11.8 ms (2.5 KiB)** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Measurement Errors

- math_vec/Sable: benchmark crashed (3221225501)

- state_vec/Sable: benchmark crashed (3221225501)

- branch_vec/Sable: benchmark crashed (3221225501)

- general_vec/Sable: benchmark crashed (3221225501)

- hft_vec/Sable: benchmark crashed (3221225501)

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 16ms / **33.98ms** | 9ms / **41.03ms** | 10ms / **48.15ms** | 10ms / **22.77ms** | 11ms / **282.79ms** | — | — | — | — | — |
| **Windows AMD64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 17ms / **38.95ms** | 7ms / **32.84ms** | 7ms / **61.60ms** | 8ms / **20.10ms** | 9ms / **323.44ms** | 8ms / **109.99ms** | 8ms / **141.90ms** | 8ms / **111.30ms** | 8ms / **73.95ms** | 8ms / **101.03ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 132ms / **64.44ms** | 30ms / **51.80ms** | 36ms / **48.42ms** | 38ms / **31.73ms** | 33ms / **581.19ms** | — | — | — | — | — |
| **Linux x86_64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 30ms / **55.61ms** | 39ms / **49.84ms** | 29ms / **79.33ms** | 31ms / **37.23ms** | 29ms / **343.02ms** | 37ms / **155.58ms** | 29ms / **192.57ms** | 27ms / **162.94ms** | 28ms / **106.20ms** | 29ms / **138.77ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42715050078) · Source: `7f9947e54e80`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

