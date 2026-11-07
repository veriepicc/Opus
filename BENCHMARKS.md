# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **6.25 ms** | 32.31 ms | 7.14 ms | 134.31 ms | **Opus** |
| **State Accumulation (50M iter)** | **5.51 ms** | 23.21 ms | 6.88 ms | 85.59 ms | **Opus** |
| **Branch Parity (40M iter)** | **5.18 ms** | 33.00 ms | 22.18 ms | 52.34 ms | **Opus** |
| **General Logic (6M iter)** | 23.19 ms | 20.87 ms | **20.25 ms** | 51.86 ms | **GCC** |
| **HFT Order Book (12M iter)** | 432.53 ms | 142.31 ms | **133.26 ms** | 403.14 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 22.4 ms (4.5 KiB) | 6931.4 ms (105.0 KiB) | 3978.2 ms (14.5 KiB) | **17.5 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **10.5 ms (4.5 KiB)** | 126.9 ms (105.0 KiB) | 118.0 ms (14.5 KiB) | 12.9 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **10.2 ms (4.5 KiB)** | 119.3 ms (105.0 KiB) | 452.0 ms (14.5 KiB) | 12.0 ms (2.5 KiB) |
| **General Logic (6M iter)** | **11.0 ms (4.5 KiB)** | 188.7 ms (105.5 KiB) | 223.7 ms (14.5 KiB) | 12.6 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **11.5 ms (7.0 KiB)** | 150.1 ms (106.0 KiB) | 418.4 ms (15.0 KiB) | 12.9 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 22ms / **6.25ms** | 10ms / **5.51ms** | 10ms / **5.18ms** | 11ms / **23.19ms** | 11ms / **432.53ms** |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 13ms / **2.48ms** | 6ms / **2.42ms** | 6ms / **2.38ms** | 6ms / **15.69ms** | 6ms / **216.04ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 114ms / **13.14ms** | 28ms / **14.53ms** | 29ms / **13.29ms** | 37ms / **30.17ms** | 33ms / **578.62ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 31ms / **23.57ms** | 33ms / **23.36ms** | 32ms / **23.17ms** | 36ms / **42.82ms** | 35ms / **442.75ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42401782794) · Source: `ef0b3a12f1a4`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

