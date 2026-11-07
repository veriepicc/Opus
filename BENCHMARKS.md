# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (4.45ms) | **Opus** (14.5ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (4.38ms) | **Opus** (8.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (4.28ms) | **Opus** (9.0ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (17.82ms) | **Opus** (9.1ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (132.20ms) | **TCC** (11.2ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 14.5 | 4,608 B | 4.45 | 5.04 | **Fastest** |
| **GCC** | 3269.4 | 14,848 B | 5.59 | 5.71 | 1.25x baseline |
| **MSVC** | 3689.1 | 107,520 B | 31.04 | 31.10 | 6.97x baseline |
| **TCC** | 15.2 | 2,560 B | 131.31 | 133.29 | 29.48x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.9 | 4,608 B | 4.38 | 4.45 | **Fastest** |
| **GCC** | 100.7 | 14,848 B | 5.57 | 6.05 | 1.27x baseline |
| **MSVC** | 102.5 | 107,520 B | 21.04 | 21.17 | 4.80x baseline |
| **TCC** | 9.8 | 2,560 B | 84.17 | 86.90 | 19.20x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.0 | 4,608 B | 4.28 | 4.38 | **Fastest** |
| **GCC** | 360.3 | 14,848 B | 20.40 | 20.61 | 4.77x baseline |
| **MSVC** | 98.2 | 107,520 B | 30.65 | 32.09 | 7.16x baseline |
| **TCC** | 9.7 | 2,560 B | 50.70 | 51.02 | 11.85x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.1 | 4,608 B | 21.57 | 22.08 | 1.21x baseline |
| **GCC** | 166.6 | 14,848 B | 17.82 | 17.92 | **Fastest** |
| **MSVC** | 115.6 | 108,032 B | 18.54 | 18.94 | 1.04x baseline |
| **TCC** | 10.1 | 2,560 B | 47.96 | 49.00 | 2.69x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 11.5 | 7,168 B | 428.34 | 433.72 | 3.24x baseline |
| **GCC** | 205.0 | 15,360 B | 132.20 | 134.35 | **Fastest** |
| **MSVC** | 129.2 | 108,544 B | 140.90 | 141.62 | 1.07x baseline |
| **TCC** | 11.2 | 4,096 B | 400.68 | 402.52 | 3.03x baseline |

## Reproduction

```bash
python benchmarks/run.py
```
