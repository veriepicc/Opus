# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **5.04 ms** | 32.47 ms | 6.75 ms | 131.81 ms | **Opus** |
| **State Accumulation (50M iter)** | **5.57 ms** | 23.09 ms | 7.01 ms | 85.35 ms | **Opus** |
| **Branch Parity (40M iter)** | **5.48 ms** | 33.11 ms | 22.79 ms | 54.62 ms | **Opus** |
| **General Logic (6M iter)** | 23.53 ms | 20.24 ms | **19.99 ms** | 49.98 ms | **GCC** |
| **HFT Order Book (12M iter)** | 429.29 ms | 142.00 ms | **132.93 ms** | 402.43 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 21.4 ms (4.5 KiB) | 3676.3 ms (105.0 KiB) | 3171.5 ms (14.5 KiB) | **16.7 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | 12.6 ms (4.5 KiB) | 121.8 ms (105.0 KiB) | 119.5 ms (14.5 KiB) | **11.8 ms (2.5 KiB)** |
| **Branch Parity (40M iter)** | **10.8 ms (4.5 KiB)** | 121.8 ms (105.0 KiB) | 406.5 ms (14.5 KiB) | 13.3 ms (2.5 KiB) |
| **General Logic (6M iter)** | **11.2 ms (4.5 KiB)** | 142.5 ms (105.5 KiB) | 131.4 ms (14.5 KiB) | 11.9 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | 13.0 ms (7.0 KiB) | 137.2 ms (106.0 KiB) | 144.3 ms (15.0 KiB) | **11.9 ms (4.0 KiB)** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 21ms / **5.04ms** | 13ms / **5.57ms** | 11ms / **5.48ms** | 11ms / **23.53ms** | 13ms / **429.29ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 11ms / **3.01ms** | 6ms / **3.02ms** | 6ms / **2.97ms** | 7ms / **19.86ms** | 7ms / **419.51ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 114ms / **14.06ms** | 28ms / **13.53ms** | 29ms / **14.02ms** | 37ms / **30.38ms** | 31ms / **579.57ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 31ms / **22.08ms** | 31ms / **22.84ms** | 131ms / **23.07ms** | 31ms / **39.12ms** | 36ms / **442.01ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42418165124) · Source: `53df54b229b8`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

