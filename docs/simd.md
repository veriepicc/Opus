# SIMD Support

Sable automatically vectorizes `parallel for` loops into AVX2 / AVX-512 instructions across a persistent worker pool, and also exposes low-level AVX2 (256-bit) and AVX-512 (512-bit) intrinsics.

For normal numeric array loops, prefer `parallel for` over manual intrinsics.

## Feature Detection

```c
function int main() {
    if simd_has_avx2() != 0 {
        print("AVX2 available\n")
    }
    if simd_has_avx512f() != 0 {
        print("AVX-512 F available\n")
    }
    if simd_has_avx512dq() != 0 {
        print("AVX-512 DQ (vpmadd52) available\n")
    }
    return 0
}
```

## Explicit SIMD Intrinsics

The compiler exposes low-level SIMD builtins directly:

| Intrinsic | Width | Operation |
|-----------|-------|-----------|
| `simd_i32x8_add` | 256-bit (8x i32) | packed add |
| `simd_i32x8_sub` | 256-bit (8x i32) | packed subtract |
| `simd_i32x8_mul` | 256-bit (8x i32) | packed multiply |
| `simd_i64x4_add` | 256-bit (4x i64) | packed add |
| `simd_i64x4_sub` | 256-bit (4x i64) | packed subtract |
| `simd_i32x16_add` | 512-bit (16x i32) | packed add |
| `simd_i32x16_mul` | 512-bit (16x i32) | packed multiply |
| `simd_i64x8_add` | 512-bit (8x i64) | packed add |
| `simd_i64x8_sub` | 512-bit (8x i64) | packed subtract |
| `simd_i64x8_mul_avx512` | 512-bit (8x i64) | vpmadd52 (HFT-optimized) |
| `simd_i64x8_mul_avx2` | 256-bit (4x i64) | vpmadd52 |
| `simd_i64x4_add_avx512` | 512-bit (8x i64) | packed add |

## Direct Builtin Calls

```c
function int main() {
    let dst = malloc(64)
    let a = malloc(64)
    let b = malloc(64)

    simd_i64x4_add(dst, a, b)
    simd_i64x8_add(dst, a, b)
    simd_i32x16_add(dst, a, b)

    free(dst)
    free(a)
    free(b)
    return 0
}
```

## Auto-Selecting Variants

```c
function void i64x8_mul_auto(ptr dst, ptr a, ptr b) {
    if simd_has_avx512dq() != 0 {
        simd_i64x8_mul_avx512(dst, a, b)
    } else {
        simd_i64x8_mul_avx2(dst, a, b)
    }
}
```

## HFT Workloads

For high-frequency trading, `vpmadd52` is the gold standard:

```c
function int main() {
    let dst = malloc(64)
    let a = malloc(64)
    let b = malloc(64)

    if simd_has_avx512dq() != 0 {
        simd_i64x8_mul_avx512(dst, a, b)  // vpmadd52 on AVX-512
    } else {
        simd_i64x8_mul_avx2(dst, a, b)   // vpmadd52 on AVX2
    }

    free(dst)
    free(a)
    free(b)
    return 0
}
```

## Requirements

| Extension | Minimum | Use Case |
|-----------|---------|----------|
| AVX2 | Haswell (2013) | general SIMD, 256-bit |
| AVX-512F | Skylake-X (2017) | 512-bit, better throughput |
| AVX-512DQ | Cascade Lake (2019) | vpmadd52, HFT workloads |

Check at runtime with `simd_has_avx2()`, `simd_has_avx512f()`, `simd_has_avx512dq()`.

## Compatibility

AVX2 is widely supported on modern CPUs. AVX-512 requires newer hardware but is available on most server and high-end desktop chips since 2017.

For maximum compatibility, use the auto-selecting variants or check features at runtime.