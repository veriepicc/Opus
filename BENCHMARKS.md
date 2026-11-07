# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 25.6ms<br>4.5 KiB<br>**4.61ms** | 3725.5ms<br>105.0 KiB<br>32.06ms | 5175.8ms<br>14.5 KiB<br>5.87ms | 15.8ms<br>2.5 KiB<br>131.97ms | **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 9.5ms<br>4.5 KiB<br>**4.78ms** | 111.0ms<br>105.0 KiB<br>22.59ms | 108.1ms<br>14.5 KiB<br>6.14ms | 11.0ms<br>2.5 KiB<br>85.17ms | **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 9.5ms<br>4.5 KiB<br>**4.76ms** | 106.6ms<br>105.0 KiB<br>31.75ms | 382.9ms<br>14.5 KiB<br>21.47ms | 11.1ms<br>2.5 KiB<br>51.78ms | **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 10.2ms<br>4.5 KiB<br>22.38ms | 180.8ms<br>105.5 KiB<br>19.64ms | 184.6ms<br>14.5 KiB<br>**18.82ms** | 11.1ms<br>2.5 KiB<br>49.07ms | **GCC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 11.2ms<br>7.0 KiB<br>427.01ms | 135.2ms<br>106.0 KiB<br>141.63ms | 200.8ms<br>15.0 KiB<br>**132.78ms** | 11.8ms<br>4.0 KiB<br>400.69ms | **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 4.61 ms | 4.78 ms | 4.76 ms | 22.38 ms | 427.01 ms |
| **Windows AMD64** | `INTEL(R) XEON(R) PLATINUM 8573C` | 3.39 ms | 3.34 ms | 3.33 ms | 19.14 ms | 232.89 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 14.67 ms | 14.78 ms | 13.91 ms | 30.94 ms | 578.97 ms |
| **Linux x86_64** | `Intel(R) Xeon(R) 6973P-C` | 24.76 ms | 22.13 ms | 20.16 ms | 31.22 ms | 222.49 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42399973206) · Source: `7e41415d8a96`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

