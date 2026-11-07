
## Finding `3cab5458b686` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:55:44Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_3cab5458b686.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_33517 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_16814(int a_46634, int b_3812) {
    a_46634 = 255;
    b_3812 = 1000;
    switch a_46634 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    a_46634 = a_46634;
    if ((b_3812 / a_46634) < (a_46634 | (5 ^ 2))) {
        print_int(b_3812);
    } else {
        for i_73239 in 0..5 {
            if false {
                print_int(i_73239);
            }
        }
    }
    b_3812 = ((b_3812 % b_3812) % b_3812);
    return -5;
}

function int main() {
    let res = helper_16814(10, 20);
    print_int(res);
    let x_51347 = 5;
    let x_55730 = ((x_51347 + x_51347) - x_51347);
    for i_59700 in 0..5 {
        i_59700 = (0 - -1);
    }
    return 0;
}
```

### Compiler Output:
```text
0
```
---

## Finding `451a3a6dbffc` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:55:51Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_451a3a6dbffc.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_73949 {
    x: int;
    y: int;
}

view Header_29126 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_24685(int a_14107, int b_80173) {
    let x_1987 = a_14107;
    for i_71612 in 1..10 {
        i_71612 = ((-42 ^ 255) ^ b_80173);
    }
    if true {
        let x_66340 = (b_80173 & (i_71612 - 5));
    }
    return x_1987;
}

function int main() {
    let res = helper_24685(10, 20);
    print_int(res);
    for i_32920 in 0..5 {
        let x_66552 = i_32920;
    }
    let x_6968 = (5 * 100);
    return 0;
}
```

### Compiler Output:
```text

```
---
