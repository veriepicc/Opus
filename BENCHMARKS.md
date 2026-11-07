# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V45 96-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 17.6ms<br>4.5 KiB<br>**3.17ms** | 4009.5ms<br>105.0 KiB<br>22.74ms | 3781.5ms<br>14.5 KiB<br>3.78ms | 10.8ms<br>2.5 KiB<br>79.97ms | 🏆 **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 7.6ms<br>4.5 KiB<br>**3.01ms** | 81.6ms<br>105.0 KiB<br>16.37ms | 74.2ms<br>14.5 KiB<br>3.88ms | 8.3ms<br>2.5 KiB<br>52.29ms | 🏆 **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 7.5ms<br>4.5 KiB<br>**3.23ms** | 106.5ms<br>105.0 KiB<br>14.59ms | 795.2ms<br>14.5 KiB<br>14.08ms | 8.1ms<br>2.5 KiB<br>28.70ms | 🏆 **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 7.1ms<br>4.5 KiB<br>13.18ms | 90.8ms<br>105.5 KiB<br>**11.61ms** | 152.0ms<br>14.5 KiB<br>12.73ms | 8.5ms<br>2.5 KiB<br>25.43ms | ⚡ **MSVC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 7.6ms<br>7.0 KiB<br>159.98ms | 99.3ms<br>106.0 KiB<br>92.75ms | 163.4ms<br>15.0 KiB<br>**86.29ms** | 8.0ms<br>4.0 KiB<br>170.85ms | ⚡ **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 3.17 ms | 3.01 ms | 3.23 ms | 13.18 ms | 159.98 ms |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 3.45 ms | 3.24 ms | 3.35 ms | 20.79 ms | 420.79 ms |
| **Windows AMD64** | `Cobalt 100` | 14.80 ms | 14.45 ms | 14.88 ms | 31.26 ms | 579.73 ms |
| **Linux x86_64** | `AMD EPYC 9V45 96-Core Processor` | 1358.71 ms | 1349.07 ms | 1363.19 ms | 1360.40 ms | 1371.55 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42393341226) · Source: `62cecb22cbb8`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

