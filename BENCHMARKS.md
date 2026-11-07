# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **2.86 ms** | 22.71 ms | 4.17 ms | 81.59 ms | **Sable** |
| **State Accumulation (50M iter)** | **2.91 ms** | 15.95 ms | 3.78 ms | 51.86 ms | **Sable** |
| **Branch Parity (40M iter)** | **2.89 ms** | 13.56 ms | 13.31 ms | 29.65 ms | **Sable** |
| **General Logic (6M iter)** | 13.04 ms | **11.68 ms** | 12.65 ms | 25.66 ms | **MSVC** |
| **HFT Order Book (12M iter)** | 153.93 ms | 96.24 ms | **87.70 ms** | 172.96 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 15.3 ms (4.5 KiB) | 4067.6 ms (105.0 KiB) | 14666.1 ms (14.5 KiB) | **11.0 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | 8.3 ms (4.5 KiB) | 79.0 ms (105.0 KiB) | 68.4 ms (14.5 KiB) | **7.2 ms (2.5 KiB)** |
| **Branch Parity (40M iter)** | 7.0 ms (4.5 KiB) | 75.6 ms (105.0 KiB) | 351.5 ms (14.5 KiB) | **6.8 ms (2.5 KiB)** |
| **General Logic (6M iter)** | **6.3 ms (4.5 KiB)** | 92.1 ms (105.5 KiB) | 131.5 ms (14.5 KiB) | 7.1 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **7.0 ms (7.0 KiB)** | 96.2 ms (106.0 KiB) | 163.8 ms (15.0 KiB) | 7.9 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 15ms / **2.86ms** | 8ms / **2.91ms** | 7ms / **2.89ms** | 6ms / **13.04ms** | 7ms / **153.93ms** |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 21ms / **3.14ms** | 7ms / **3.17ms** | 7ms / **3.17ms** | 7ms / **20.70ms** | 8ms / **421.51ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 104ms / **13.64ms** | 29ms / **14.13ms** | 28ms / **13.27ms** | 36ms / **31.12ms** | 30ms / **580.69ms** |
| **Linux x86_64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 37ms / **23.06ms** | 30ms / **21.76ms** | 74ms / **22.84ms** | 86ms / **37.05ms** | 78ms / **223.55ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42424114218) · Source: `efe086c2e11c`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

