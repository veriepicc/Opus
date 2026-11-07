# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (3.41ms) | **TCC** (12.0ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.48ms) | **TCC** (8.1ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.48ms) | **Opus** (7.7ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (15.64ms) | **Opus** (8.0ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (112.40ms) | **Opus** (8.2ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 16.3 | 4,608 B | 3.41 | 3.47 | **Fastest** |
| **GCC** | 3352.2 | 14,848 B | 4.56 | 4.84 | 1.34x baseline |
| **MSVC** | 4066.8 | 107,520 B | 26.67 | 28.23 | 7.82x baseline |
| **TCC** | 12.0 | 2,560 B | 115.05 | 119.15 | 33.72x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.4 | 4,608 B | 3.48 | 3.56 | **Fastest** |
| **GCC** | 84.3 | 14,848 B | 4.43 | 4.58 | 1.27x baseline |
| **MSVC** | 92.5 | 107,520 B | 18.26 | 18.53 | 5.25x baseline |
| **TCC** | 8.1 | 2,560 B | 73.73 | 74.93 | 21.18x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.7 | 4,608 B | 3.48 | 3.55 | **Fastest** |
| **GCC** | 399.0 | 14,848 B | 17.63 | 17.92 | 5.06x baseline |
| **MSVC** | 87.9 | 107,520 B | 27.03 | 27.37 | 7.76x baseline |
| **TCC** | 8.4 | 2,560 B | 39.87 | 40.29 | 11.45x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.0 | 4,608 B | 16.98 | 17.29 | 1.09x baseline |
| **GCC** | 157.9 | 14,848 B | 15.64 | 16.05 | **Fastest** |
| **MSVC** | 105.9 | 108,032 B | 16.34 | 16.65 | 1.04x baseline |
| **TCC** | 8.4 | 2,560 B | 38.71 | 39.16 | 2.48x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.2 | 7,168 B | 219.56 | 223.34 | 1.95x baseline |
| **GCC** | 183.9 | 15,360 B | 112.40 | 112.71 | **Fastest** |
| **MSVC** | 112.2 | 108,544 B | 120.39 | 120.52 | 1.07x baseline |
| **TCC** | 8.7 | 4,096 B | 258.25 | 260.30 | 2.30x baseline |

## Reproduction

```bash
python benchmarks/run.py
```
