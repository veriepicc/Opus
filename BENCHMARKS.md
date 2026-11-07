# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 7763 64-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 20.8ms<br>4.5 KiB<br>**4.66ms** | 6368.7ms<br>105.0 KiB<br>31.11ms | 3499.7ms<br>14.5 KiB<br>5.81ms | 15.9ms<br>2.5 KiB<br>131.96ms | **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 9.3ms<br>4.5 KiB<br>**4.42ms** | 111.0ms<br>105.0 KiB<br>21.44ms | 105.6ms<br>14.5 KiB<br>5.92ms | 10.0ms<br>2.5 KiB<br>84.74ms | **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 9.0ms<br>4.5 KiB<br>**4.47ms** | 108.1ms<br>105.0 KiB<br>30.99ms | 412.2ms<br>14.5 KiB<br>20.51ms | 10.6ms<br>2.5 KiB<br>51.17ms | **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 11.0ms<br>4.5 KiB<br>21.82ms | 122.6ms<br>105.5 KiB<br>18.85ms | 177.2ms<br>14.5 KiB<br>**18.29ms** | 11.3ms<br>2.5 KiB<br>48.03ms | **GCC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 9.9ms<br>7.0 KiB<br>428.44ms | 128.5ms<br>106.0 KiB<br>141.42ms | 203.4ms<br>15.0 KiB<br>**132.71ms** | 11.4ms<br>4.0 KiB<br>407.96ms | **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 4.66 ms | 4.42 ms | 4.47 ms | 21.82 ms | 428.44 ms |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 2.15 ms | 2.08 ms | 2.07 ms | 11.81 ms | 143.71 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 15.82 ms | 14.92 ms | 14.68 ms | 31.40 ms | 578.66 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42397766021) · Source: `4b4423fdd042`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

