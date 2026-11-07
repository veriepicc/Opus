# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (4.45ms) | **TCC** (14.7ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (4.32ms) | **TCC** (9.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (4.64ms) | **Opus** (9.6ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (18.03ms) | **Opus** (9.5ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (132.22ms) | **Opus** (9.7ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 22.1 | 4,608 B | 4.45 | 4.55 | **Fastest** |
| **GCC** | 3490.5 | 14,848 B | 5.60 | 5.72 | 1.26x baseline |
| **MSVC** | 4000.9 | 107,520 B | 30.82 | 31.11 | 6.92x baseline |
| **TCC** | 14.7 | 2,560 B | 131.90 | 132.59 | 29.62x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 10.1 | 4,608 B | 4.32 | 4.47 | **Fastest** |
| **GCC** | 102.8 | 14,848 B | 5.58 | 5.67 | 1.29x baseline |
| **MSVC** | 113.1 | 107,520 B | 21.07 | 21.34 | 4.88x baseline |
| **TCC** | 9.9 | 2,560 B | 83.81 | 85.27 | 19.40x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.6 | 4,608 B | 4.64 | 4.85 | **Fastest** |
| **GCC** | 393.3 | 14,848 B | 20.32 | 22.67 | 4.37x baseline |
| **MSVC** | 105.0 | 107,520 B | 30.77 | 30.99 | 6.62x baseline |
| **TCC** | 10.5 | 2,560 B | 51.29 | 53.71 | 11.04x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.5 | 4,608 B | 21.39 | 21.66 | 1.19x baseline |
| **GCC** | 170.9 | 14,848 B | 18.03 | 18.40 | **Fastest** |
| **MSVC** | 117.1 | 108,032 B | 18.91 | 19.08 | 1.05x baseline |
| **TCC** | 10.1 | 2,560 B | 48.01 | 50.76 | 2.66x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.7 | 7,168 B | 427.19 | 430.17 | 3.23x baseline |
| **GCC** | 210.0 | 15,360 B | 132.22 | 134.33 | **Fastest** |
| **MSVC** | 134.7 | 108,544 B | 141.27 | 143.60 | 1.07x baseline |
| **TCC** | 12.2 | 4,096 B | 402.91 | 404.93 | 3.05x baseline |

## Reproduction

```bash
python benchmarks/run.py
```
