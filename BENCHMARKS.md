# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 19.3ms<br>4.5 KiB<br>**4.43ms** | 3801.6ms<br>105.0 KiB<br>30.43ms | 3565.0ms<br>14.5 KiB<br>5.66ms | 14.4ms<br>2.5 KiB<br>130.79ms | **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 9.1ms<br>4.5 KiB<br>**4.36ms** | 108.8ms<br>105.0 KiB<br>21.18ms | 100.8ms<br>14.5 KiB<br>5.60ms | 10.1ms<br>2.5 KiB<br>83.84ms | **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 8.6ms<br>4.5 KiB<br>**4.33ms** | 98.1ms<br>105.0 KiB<br>30.52ms | 362.7ms<br>14.5 KiB<br>20.28ms | 9.9ms<br>2.5 KiB<br>50.57ms | **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 9.1ms<br>4.5 KiB<br>21.37ms | 119.9ms<br>105.5 KiB<br>18.58ms | 173.3ms<br>14.5 KiB<br>**17.90ms** | 10.2ms<br>2.5 KiB<br>47.54ms | **GCC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 10.6ms<br>7.0 KiB<br>426.70ms | 130.2ms<br>106.0 KiB<br>141.33ms | 206.7ms<br>15.0 KiB<br>**131.56ms** | 11.0ms<br>4.0 KiB<br>400.54ms | **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 4.43 ms | 4.36 ms | 4.33 ms | 21.37 ms | 426.70 ms |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 3.40 ms | 3.29 ms | 3.15 ms | 20.38 ms | 420.19 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 13.92 ms | 13.69 ms | 13.92 ms | 30.40 ms | 579.25 ms |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 1507.49 ms | 1508.31 ms | 1499.00 ms | 1500.88 ms | 1509.59 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42398789888) · Source: `68461aeb48ca`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

