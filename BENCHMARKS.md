# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **3.00 ms** | 21.22 ms | 3.74 ms | 75.08 ms | **Opus** |
| **State Accumulation (50M iter)** | **2.69 ms** | 15.70 ms | 3.66 ms | 47.95 ms | **Opus** |
| **Branch Parity (40M iter)** | **2.62 ms** | 13.96 ms | 13.58 ms | 29.76 ms | **Opus** |
| **General Logic (6M iter)** | 12.37 ms | **11.10 ms** | 12.76 ms | 25.26 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 144.88 ms | 89.61 ms | **82.54 ms** | 162.70 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 24.0 ms (4.5 KiB) | 4090.1 ms (105.0 KiB) | 3669.9 ms (14.5 KiB) | **9.2 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **6.7 ms (4.5 KiB)** | 77.7 ms (105.0 KiB) | 65.2 ms (14.5 KiB) | 8.0 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **5.6 ms (4.5 KiB)** | 74.8 ms (105.0 KiB) | 345.0 ms (14.5 KiB) | 7.9 ms (2.5 KiB) |
| **General Logic (6M iter)** | **6.9 ms (4.5 KiB)** | 90.1 ms (105.5 KiB) | 143.5 ms (14.5 KiB) | 7.3 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | 7.6 ms (7.0 KiB) | 101.3 ms (106.0 KiB) | 166.9 ms (15.0 KiB) | **7.0 ms (4.0 KiB)** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 24ms / **3.00ms** | 7ms / **2.69ms** | 6ms / **2.62ms** | 7ms / **12.37ms** | 8ms / **144.88ms** |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 9ms / **2.43ms** | 6ms / **2.41ms** | 6ms / **2.40ms** | 6ms / **15.69ms** | 7ms / **215.76ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 117ms / **14.28ms** | 29ms / **14.19ms** | 29ms / **14.41ms** | 41ms / **31.02ms** | 33ms / **577.77ms** |
| **Linux x86_64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 36ms / **20.75ms** | 33ms / **21.33ms** | 30ms / **21.50ms** | 31ms / **38.98ms** | 28ms / **341.81ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42402978739) · Source: `e734f72ccb2e`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

