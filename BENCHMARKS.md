# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **2.71 ms** | 21.19 ms | 3.50 ms | 74.56 ms | **Opus** |
| **State Accumulation (50M iter)** | **2.62 ms** | 14.48 ms | 3.62 ms | 47.73 ms | **Opus** |
| **Branch Parity (40M iter)** | **2.63 ms** | 13.51 ms | 12.49 ms | 27.75 ms | **Opus** |
| **General Logic (6M iter)** | 12.10 ms | **10.82 ms** | 11.74 ms | 23.67 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 143.97 ms | 88.70 ms | **81.30 ms** | 165.53 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **9.6 ms (4.5 KiB)** | 3537.8 ms (105.0 KiB) | 3107.2 ms (14.5 KiB) | 20.3 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **5.3 ms (4.5 KiB)** | 73.6 ms (105.0 KiB) | 61.0 ms (14.5 KiB) | 6.3 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **5.3 ms (4.5 KiB)** | 76.0 ms (105.0 KiB) | 327.5 ms (14.5 KiB) | 6.2 ms (2.5 KiB) |
| **General Logic (6M iter)** | **6.1 ms (4.5 KiB)** | 85.8 ms (105.5 KiB) | 129.7 ms (14.5 KiB) | 6.3 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **6.0 ms (7.0 KiB)** | 91.2 ms (106.0 KiB) | 148.4 ms (15.0 KiB) | 7.0 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 10ms / **2.71ms** | 5ms / **2.62ms** | 5ms / **2.63ms** | 6ms / **12.10ms** | 6ms / **143.97ms** |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 14ms / **2.43ms** | 6ms / **2.42ms** | 6ms / **2.42ms** | 6ms / **15.67ms** | 7ms / **216.07ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 104ms / **13.99ms** | 28ms / **13.72ms** | 28ms / **14.36ms** | 37ms / **30.98ms** | 32ms / **579.29ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 32ms / **23.81ms** | 36ms / **23.79ms** | 37ms / **23.53ms** | 32ms / **40.66ms** | 160ms / **443.12ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42403711803) · Source: `3df5e31bb948`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

