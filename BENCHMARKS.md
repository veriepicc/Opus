# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Opus | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.73 ms** | 30.50 ms | 5.52 ms | 130.70 ms | **Opus** |
| **State Accumulation (50M iter)** | **4.61 ms** | 20.85 ms | 5.43 ms | 83.76 ms | **Opus** |
| **Branch Parity (40M iter)** | **4.19 ms** | 30.43 ms | 20.09 ms | 50.41 ms | **Opus** |
| **General Logic (6M iter)** | 21.25 ms | 18.54 ms | **17.74 ms** | 47.43 ms | **GCC** |
| **HFT Order Book (12M iter)** | 427.83 ms | 140.72 ms | **131.20 ms** | 400.59 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Opus | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 18.4 ms (4.5 KiB) | 3165.6 ms (105.0 KiB) | 2745.0 ms (14.5 KiB) | **14.6 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **8.4 ms (4.5 KiB)** | 99.8 ms (105.0 KiB) | 100.2 ms (14.5 KiB) | 9.8 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **8.2 ms (4.5 KiB)** | 94.7 ms (105.0 KiB) | 350.8 ms (14.5 KiB) | 9.5 ms (2.5 KiB) |
| **General Logic (6M iter)** | **8.9 ms (4.5 KiB)** | 118.3 ms (105.5 KiB) | 155.3 ms (14.5 KiB) | 9.4 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | 11.0 ms (7.0 KiB) | 122.4 ms (106.0 KiB) | 188.8 ms (15.0 KiB) | **10.8 ms (4.0 KiB)** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 18ms / **4.73ms** | 8ms / **4.61ms** | 8ms / **4.19ms** | 9ms / **21.25ms** | 11ms / **427.83ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 20ms / **3.03ms** | 7ms / **3.02ms** | 6ms / **3.02ms** | 7ms / **19.79ms** | 8ms / **420.90ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 99ms / **14.74ms** | 31ms / **15.00ms** | 29ms / **14.71ms** | 38ms / **31.21ms** | 33ms / **580.26ms** |
| **Linux x86_64** | `AMD EPYC 9V45 96-Core Processor` | 26ms / **16.76ms** | 26ms / **16.49ms** | 23ms / **16.82ms** | 64ms / **26.67ms** | 74ms / **167.08ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42402350788) · Source: `1cf2e032fb77`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

