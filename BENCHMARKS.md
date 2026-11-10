# Benchmark Showdown

- **Date:** 2026-11-10
- **Host:** `Windows AMD64`
- **CPU:** `INTEL(R) XEON(R) PLATINUM 8573C`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.61 ms** | 19.72 ms | 6.09 ms | 106.85 ms | **Sable** |
| **State Accumulation (50M iter)** | 38.91 ms | 23.04 ms | **5.84 ms** | 64.99 ms | **GCC** |
| **Branch Parity (40M iter)** | 69.60 ms | 33.13 ms | **19.87 ms** | 64.52 ms | **GCC** |
| **General Logic (6M iter)** | 21.39 ms | 20.27 ms | **15.73 ms** | 41.77 ms | **GCC** |
| **HFT Order Book (12M iter)** | 237.50 ms | 130.01 ms | **117.16 ms** | 266.34 ms | **GCC** |
| **Math SIMD Vector (40M items)** | **120.99 ms** | 244.83 ms | 237.83 ms | 332.34 ms | **Sable** |
| **State SIMD Vector (40M items)** | **172.94 ms** | 364.85 ms | 363.38 ms | 474.94 ms | **Sable** |
| **Branch SIMD Vector (40M items)** | **126.11 ms** | 240.69 ms | 235.97 ms | 310.85 ms | **Sable** |
| **General SIMD Vector (20M items)** | **87.74 ms** | 183.32 ms | 182.57 ms | 259.39 ms | **Sable** |
| **HFT SIMD Vector (20M items)** | **103.89 ms** | 192.06 ms | 193.32 ms | 238.96 ms | **Sable** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **14.5 ms (4.5 KiB)** | 4551.8 ms (105.0 KiB) | 7851.9 ms (14.5 KiB) | 16.6 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **9.3 ms (4.5 KiB)** | 111.2 ms (105.0 KiB) | 115.3 ms (14.5 KiB) | 10.9 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **9.2 ms (4.5 KiB)** | 108.0 ms (105.0 KiB) | 1276.0 ms (14.5 KiB) | 10.6 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.0 ms (4.5 KiB)** | 136.4 ms (105.5 KiB) | 388.9 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | 11.8 ms (7.0 KiB) | 168.7 ms (106.0 KiB) | 474.5 ms (15.0 KiB) | **11.7 ms (4.0 KiB)** |
| **Math SIMD Vector (40M items)** | **9.8 ms (8.0 KiB)** | 219.9 ms (106.0 KiB) | 733.3 ms (15.0 KiB) | 11.9 ms (2.5 KiB) |
| **State SIMD Vector (40M items)** | **9.6 ms (10.5 KiB)** | 130.7 ms (106.0 KiB) | 142.3 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **Branch SIMD Vector (40M items)** | **10.1 ms (8.5 KiB)** | 144.3 ms (106.0 KiB) | 161.7 ms (15.0 KiB) | 11.4 ms (2.5 KiB) |
| **General SIMD Vector (20M items)** | **10.0 ms (11.5 KiB)** | 113.0 ms (106.0 KiB) | 116.6 ms (14.5 KiB) | 11.6 ms (2.5 KiB) |
| **HFT SIMD Vector (20M items)** | **9.9 ms (10.0 KiB)** | 147.3 ms (106.5 KiB) | 139.5 ms (15.0 KiB) | 12.2 ms (2.5 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

### Scalar Benchmarks
| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) | Math Vec (40M) | State Vec (40M) | Branch Vec (40M) | General Vec (20M) | HFT Vec (20M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 15ms / **4.61ms** | 9ms / **38.91ms** | 9ms / **69.60ms** | 10ms / **21.39ms** | 12ms / **237.50ms** | 10ms / **120.99ms** | 10ms / **172.94ms** | 10ms / **126.11ms** | 10ms / **87.74ms** | 10ms / **103.89ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 17ms / **3.11ms** | 7ms / **34.14ms** | 7ms / **40.62ms** | 8ms / **19.85ms** | 8ms / **420.59ms** | 7ms / **108.99ms** | 8ms / **144.84ms** | 8ms / **111.98ms** | 7ms / **80.92ms** | 8ms / **99.28ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 101ms / **15.69ms** | 30ms / **51.57ms** | 31ms / **47.85ms** | 38ms / **31.68ms** | 35ms / **578.65ms** | 38ms / **117.88ms** | 31ms / **111.50ms** | 31ms / **144.83ms** | 31ms / **84.29ms** | 32ms / **110.90ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 37ms / **22.43ms** | 31ms / **53.68ms** | 31ms / **70.19ms** | 36ms / **39.30ms** | 32ms / **442.20ms** | 37ms / **206.60ms** | 31ms / **284.44ms** | 37ms / **216.77ms** | 34ms / **155.11ms** | 32ms / **176.32ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42850410216) · Source: `18beac0a1425`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

