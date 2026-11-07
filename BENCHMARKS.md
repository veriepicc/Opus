# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.42 ms** | 30.49 ms | 5.51 ms | 131.29 ms | **Sable** |
| **State Accumulation (50M iter)** | **4.28 ms** | 21.17 ms | 5.68 ms | 84.07 ms | **Sable** |
| **Branch Parity (40M iter)** | **4.33 ms** | 31.26 ms | 20.44 ms | 50.80 ms | **Sable** |
| **General Logic (6M iter)** | 21.52 ms | 18.53 ms | **17.79 ms** | 47.69 ms | **GCC** |
| **HFT Order Book (12M iter)** | 427.83 ms | 140.78 ms | **131.77 ms** | 400.74 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 18.7 ms (4.5 KiB) | 3622.0 ms (105.0 KiB) | 3102.7 ms (14.5 KiB) | **15.0 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **9.0 ms (4.5 KiB)** | 104.2 ms (105.0 KiB) | 109.4 ms (14.5 KiB) | 9.9 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **9.1 ms (4.5 KiB)** | 100.0 ms (105.0 KiB) | 339.4 ms (14.5 KiB) | 10.3 ms (2.5 KiB) |
| **General Logic (6M iter)** | 10.9 ms (4.5 KiB) | 122.8 ms (105.5 KiB) | 180.4 ms (14.5 KiB) | **10.1 ms (2.5 KiB)** |
| **HFT Order Book (12M iter)** | **9.6 ms (7.0 KiB)** | 130.1 ms (106.0 KiB) | 198.6 ms (15.0 KiB) | 10.5 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 19ms / **4.42ms** | 9ms / **4.28ms** | 9ms / **4.33ms** | 11ms / **21.52ms** | 10ms / **427.83ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 16ms / **3.01ms** | 7ms / **2.99ms** | 7ms / **3.11ms** | 8ms / **20.40ms** | 8ms / **419.92ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 106ms / **16.08ms** | 31ms / **15.87ms** | 30ms / **15.67ms** | 40ms / **31.73ms** | 35ms / **581.96ms** |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 31ms / **22.17ms** | 35ms / **21.85ms** | 113ms / **22.09ms** | 30ms / **39.13ms** | 30ms / **441.08ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42426212342) · Source: `8a9ed2faeff7`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

