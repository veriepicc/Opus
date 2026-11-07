# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V74 80-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **3.67 ms** | 27.45 ms | 4.49 ms | 114.76 ms | **Sable** |
| **State Accumulation (50M iter)** | **3.42 ms** | 18.85 ms | 4.94 ms | 73.74 ms | **Sable** |
| **Branch Parity (40M iter)** | **3.45 ms** | 27.40 ms | 17.94 ms | 40.18 ms | **Sable** |
| **General Logic (6M iter)** | 17.22 ms | 16.61 ms | **16.05 ms** | 38.35 ms | **GCC** |
| **HFT Order Book (12M iter)** | 219.28 ms | 120.33 ms | **112.23 ms** | 258.94 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | **12.3 ms (4.5 KiB)** | 4615.7 ms (105.0 KiB) | 4063.4 ms (14.5 KiB) | 12.5 ms (2.5 KiB) |
| **State Accumulation (50M iter)** | **7.6 ms (4.5 KiB)** | 92.7 ms (105.0 KiB) | 83.5 ms (14.5 KiB) | 8.9 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **7.7 ms (4.5 KiB)** | 87.7 ms (105.0 KiB) | 460.9 ms (14.5 KiB) | 8.7 ms (2.5 KiB) |
| **General Logic (6M iter)** | **8.1 ms (4.5 KiB)** | 101.8 ms (105.5 KiB) | 153.5 ms (14.5 KiB) | 8.5 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **8.6 ms (7.0 KiB)** | 117.7 ms (106.0 KiB) | 221.2 ms (15.0 KiB) | 9.0 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 12ms / **3.67ms** | 8ms / **3.42ms** | 8ms / **3.45ms** | 8ms / **17.22ms** | 9ms / **219.28ms** |
| **Windows AMD64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 15ms / **3.06ms** | 7ms / **3.00ms** | 6ms / **3.02ms** | 7ms / **16.58ms** | 7ms / **205.56ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 90ms / **13.51ms** | 27ms / **13.75ms** | 28ms / **13.62ms** | 37ms / **30.77ms** | 32ms / **579.88ms** |
| **Linux x86_64** | `AMD EPYC 9V45 96-Core Processor` | 24ms / **16.77ms** | 33ms / **16.99ms** | 132ms / **17.31ms** | 141ms / **27.20ms** | 55ms / **165.25ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42423385826) · Source: `2cabe210e1fd`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

