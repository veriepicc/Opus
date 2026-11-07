# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 20.1ms<br>4.5 KiB<br>**4.36ms** | 4203.0ms<br>105.0 KiB<br>31.35ms | 3668.8ms<br>14.5 KiB<br>5.70ms | 15.2ms<br>2.5 KiB<br>131.15ms | **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 10.6ms<br>4.5 KiB<br>**4.29ms** | 105.3ms<br>105.0 KiB<br>21.37ms | 103.8ms<br>14.5 KiB<br>5.72ms | 10.0ms<br>2.5 KiB<br>84.77ms | **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 10.4ms<br>4.5 KiB<br>**4.47ms** | 99.2ms<br>105.0 KiB<br>30.94ms | 385.3ms<br>14.5 KiB<br>20.87ms | 10.2ms<br>2.5 KiB<br>51.23ms | **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 9.4ms<br>4.5 KiB<br>21.70ms | 118.5ms<br>105.5 KiB<br>19.00ms | 178.7ms<br>14.5 KiB<br>**18.08ms** | 10.2ms<br>2.5 KiB<br>48.28ms | **GCC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 10.4ms<br>7.0 KiB<br>427.16ms | 132.7ms<br>106.0 KiB<br>141.46ms | 209.2ms<br>15.0 KiB<br>**131.95ms** | 11.1ms<br>4.0 KiB<br>400.29ms | **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 4.36 ms | 4.29 ms | 4.47 ms | 21.70 ms | 427.16 ms |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 2.54 ms | 2.45 ms | 2.47 ms | 15.87 ms | 215.93 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 16.00 ms | 14.81 ms | 15.32 ms | 31.75 ms | 576.31 ms |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 0.01 ms | 0.01 ms | 0.01 ms | 0.01 ms | 8.33 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42395720972) · Source: `aaab0e807ad9`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

