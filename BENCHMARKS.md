# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.58 ms** | 31.24 ms | 5.74 ms | 132.14 ms | **Opus** |
| **State Accumulation (50M iter)** | **4.48 ms** | 22.20 ms | 5.80 ms | 85.14 ms | **Opus** |
| **Branch Parity (40M iter)** | **4.34 ms** | 33.00 ms | 20.90 ms | 52.17 ms | **Opus** |
| **General Logic (6M iter)** | 22.35 ms | 20.38 ms | **18.09 ms** | 48.83 ms | **GCC** |
| **HFT Order Book (12M iter)** | 428.71 ms | 142.12 ms | **133.23 ms** | 405.91 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 20.3 ms (4.5 KiB) | 3912.5 ms (105.0 KiB) | 3354.2 ms (14.5 KiB) | **14.9 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **10.1 ms (4.5 KiB)** | 113.1 ms (105.0 KiB) | 106.9 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **8.9 ms (4.5 KiB)** | 108.7 ms (105.0 KiB) | 388.5 ms (14.5 KiB) | 11.9 ms (2.5 KiB) |
| **General Logic (6M iter)** | **10.9 ms (4.5 KiB)** | 127.3 ms (105.5 KiB) | 174.8 ms (14.5 KiB) | 11.9 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **11.5 ms (7.0 KiB)** | 134.9 ms (106.0 KiB) | 215.1 ms (15.0 KiB) | 13.6 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 20ms / **4.58ms** | 10ms / **4.48ms** | 9ms / **4.34ms** | 11ms / **22.35ms** | 11ms / **428.71ms** |
| **Windows AMD64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 16ms / **3.51ms** | 8ms / **3.54ms** | 7ms / **3.44ms** | 8ms / **18.81ms** | 9ms / **236.56ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 100ms / **15.42ms** | 29ms / **14.17ms** | 31ms / **15.36ms** | 39ms / **32.18ms** | 34ms / **578.69ms** |
| **Linux x86_64** | `AMD EPYC 9V45 96-Core Processor` | 23ms / **16.26ms** | 25ms / **17.31ms** | 24ms / **17.11ms** | 26ms / **28.10ms** | 25ms / **164.99ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42400872440) · Source: `e70c2110e6bd`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

