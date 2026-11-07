# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Execution Runtime (lower is better)

| Workload | Sable | MSVC | GCC | TCC | Winner |
| :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | **4.59 ms** | 31.36 ms | 5.63 ms | 132.05 ms | **Sable** |
| **State Accumulation (50M iter)** | **4.40 ms** | 22.30 ms | 5.92 ms | 84.75 ms | **Sable** |
| **Branch Parity (40M iter)** | **4.45 ms** | 31.57 ms | 20.85 ms | 51.27 ms | **Sable** |
| **General Logic (6M iter)** | 22.20 ms | 19.39 ms | **18.45 ms** | 48.75 ms | **GCC** |
| **HFT Order Book (12M iter)** | 427.51 ms | 141.52 ms | **132.06 ms** | 399.85 ms | **GCC** |

## Compilation Speed & Binary Size

| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |
| :--- | ---: | ---: | ---: | ---: |
| **Math Loop (80M iter)** | 19.7 ms (4.5 KiB) | 3995.9 ms (105.0 KiB) | 3620.2 ms (14.5 KiB) | **14.9 ms (2.5 KiB)** |
| **State Accumulation (50M iter)** | **9.6 ms (4.5 KiB)** | 114.3 ms (105.0 KiB) | 103.6 ms (14.5 KiB) | 11.1 ms (2.5 KiB) |
| **Branch Parity (40M iter)** | **10.0 ms (4.5 KiB)** | 105.9 ms (105.0 KiB) | 373.9 ms (14.5 KiB) | 10.4 ms (2.5 KiB) |
| **General Logic (6M iter)** | **9.5 ms (4.5 KiB)** | 127.3 ms (105.5 KiB) | 179.9 ms (14.5 KiB) | 10.7 ms (2.5 KiB) |
| **HFT Order Book (12M iter)** | **10.5 ms (7.0 KiB)** | 133.0 ms (106.0 KiB) | 215.9 ms (15.0 KiB) | 10.6 ms (4.0 KiB) |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Sable: Compile / Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 20ms / **4.59ms** | 10ms / **4.40ms** | 10ms / **4.45ms** | 10ms / **22.20ms** | 10ms / **427.51ms** |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 9ms / **2.47ms** | 6ms / **2.43ms** | 6ms / **2.44ms** | 6ms / **15.69ms** | 7ms / **215.78ms** |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 93ms / **14.53ms** | 28ms / **13.54ms** | 27ms / **13.91ms** | 36ms / **30.03ms** | 31ms / **578.58ms** |
| **Linux x86_64** | `AMD EPYC 9V74 80-Core Processor` | 26ms / **19.47ms** | 80ms / **20.85ms** | 50ms / **19.41ms** | 27ms / **32.59ms** | 27ms / **233.57ms** |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Sable/actions/runs/42425082235) · Source: `82fb8eb33a4b`.

## Reproduction

```powershell
./bin/Release/sable.exe build benchmarks/benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

