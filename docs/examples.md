# Opus Examples

Runnable example programs in the `examples/` directory, organized by feature.

> EXE programs need `alloc_console()` at the start of `main()` for console output.
> `print()` does not append a newline — use `\n` explicitly.

---

## Example Files

| File | Description |
|------|-------------|
| [`hello.op`](#hello-world) | Hello world |
| [`fibonacci.op`](#fibonacci) | Recursive fibonacci |
| [`strings.op`](#strings) | String operations and character analysis |
| [`arrays.op`](#arrays) | Arrays and bubble sort |
| [`structs.op`](#structs) | Structs and nested data |
| [`classes.op`](#classes) | Classes with methods |
| [`enums.op`](#enums) | Enums and state machines |
| [`memory.op`](#memory) | Raw memory operations |
| [`concurrency.op`](#concurrency) | spawn/await and parallel for |
| [`benchmark.op`](#benchmark) | Sequential vs parallel performance |
| [`ffi.op`](#ffi) | Windows API through FFI |
| [`scanner.op`](#scanner) | DLL memory scanner |

---

## Hello World

```opus
// the simplest opus program
function int main() {
    alloc_console()
    print("Hello, World!\n")
    return 0
}
```

```
> opus hello.op
> hello.exe
Hello, World!
```

---

## Fibonacci

```opus
function int fib(int n) {
    if n <= 1 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

function int main() {
    alloc_console()

    print("fibonacci sequence:\n")
    for i in range(0, 21) {
        print("fib(")
        print_int(i)
        print(") = ")
        print_int(fib(i))
    }

    return 0
}
```

---

## Strings

```opus
function void analyze(str text) {
    let len = string_length(text)
    var digits = 0
    var letters = 0
    var spaces = 0
    var other = 0

    for i in range(0, len) {
        let ch = char_at(text, i)
        if is_digit(ch) {
            digits++
        } else if is_alpha(ch) {
            letters++
        } else if is_whitespace(ch) {
            spaces++
        } else {
            other++
        }
    }

    print("\"")
    print(text)
    print("\"\n")
    print("  length:  ") print_int(len)
    print("  letters: ") print_int(letters)
    print("  digits:  ") print_int(digits)
    print("  spaces:  ") print_int(spaces)
    print("  other:   ") print_int(other)
    print("\n")
}

function int main() {
    alloc_console()

    analyze("Hello World 123!")
    analyze("opus v0.1.0 -- systems lang")

    let greeting = string_append("hello", " world")
    print("appended: ")
    print(greeting)
    print("\n")

    let sub = string_substring(greeting, 6, 5)
    print("substring: ")
    print(sub)
    print("\n")

    free(sub)
    free(greeting)
    return 0
}
```

---

## Arrays

```opus
function void bubble_sort(ptr arr, int len) {
    for i in range(0, len) {
        for j in range(0, len - i - 1) {
            let a = arr[j]
            let b = arr[j + 1]
            if a > b {
                array_set(arr, j, b)
                array_set(arr, j + 1, a)
            }
        }
    }
}

function void print_array(ptr arr, int len) {
    print("[")
    for i in range(0, len) {
        print_int(arr[i])
        if i < len - 1 {
            print(", ")
        }
    }
    print("]\n")
}

function int main() {
    alloc_console()

    let data = array_new(10)
    array_set(data, 0, 64)
    array_set(data, 1, 25)
    array_set(data, 2, 12)
    array_set(data, 3, 99)
    array_set(data, 4, 1)
    array_set(data, 5, 47)
    array_set(data, 6, 83)
    array_set(data, 7, 36)
    array_set(data, 8, 71)
    array_set(data, 9, 8)

    print("before: ")
    print_array(data, 10)

    bubble_sort(data, 10)

    print("after:  ")
    print_array(data, 10)

    array_free(data)
    return 0
}
```

---

## Structs

```opus
struct Vec2 {
    int x;
    int y;
}

struct Rect {
    ptr origin;
    int width;
    int height;
}

function int area(ptr r) {
    return r.width * r.height
}

function int perimeter(ptr r) {
    return 2 * (r.width + r.height)
}

function void print_rect(ptr r) {
    print("rect at (")
    print_int(r.origin.x)
    print(", ")
    print_int(r.origin.y)
    print(") size ")
    print_int(r.width)
    print("x")
    print_int(r.height)
    print(" area=")
    print_int(area(r))
    print(" perim=")
    print_int(perimeter(r))
    print("\n")
}

function int main() {
    alloc_console()

    let p1 = Vec2 { x: 10, y: 20 }
    let p2 = Vec2 { x: 0, y: 0 }

    let r1 = Rect { origin: p1, width: 100, height: 50 }
    let r2 = Rect { origin: p2, width: 30, height: 30 }

    print_rect(r1)
    print_rect(r2)

    free(r1)
    free(r2)
    free(p1)
    free(p2)
    return 0
}
```

---

## Classes

```opus
class Player {
    health: int,
    armor: int,
    name: str,

    function void take_damage(int amount) {
        let absorbed = amount * self.armor / 100
        let actual = amount - absorbed
        self.health = self.health - actual
        if self.health < 0 {
            self.health = 0
        }
    }

    function void heal(int amount) {
        self.health = self.health + amount
        if self.health > 100 {
            self.health = 100
        }
    }

    function bool is_alive() {
        return self.health > 0
    }

    function void status() {
        print(self.name)
        print(" | hp: ")
        print_int(self.health)
        print(" | armor: ")
        print_int(self.armor)
        if self.is_alive() {
            print(" [alive]\n")
        } else {
            print(" [dead]\n")
        }
    }
}

function int main() {
    alloc_console()

    var warrior = Player { health: 100, armor: 60, name: "warrior" }
    var mage = Player { health: 100, armor: 10, name: "mage" }

    warrior.status()
    mage.status()

    print("\n--- combat round ---\n")
    warrior.take_damage(40)
    mage.take_damage(40)

    warrior.status()
    mage.status()

    print("\n--- healing ---\n")
    warrior.heal(20)
    mage.heal(50)

    warrior.status()
    mage.status()

    free(warrior)
    free(mage)
    return 0
}
```

---

## Enums

```opus
enum State {
    Idle,
    Running,
    Attacking,
    Dead,
}

function str state_name(int s) {
    if s == State.Idle { return "idle" }
    if s == State.Running { return "running" }
    if s == State.Attacking { return "attacking" }
    if s == State.Dead { return "dead" }
    return "unknown"
}

function int main() {
    alloc_console()

    var state = State.Idle
    var ticks = 0

    loop {
        print("tick ")
        print_int(ticks)
        print(": ")
        print(state_name(state))
        print("\n")

        if state == State.Idle {
            state = State.Running
        } else if state == State.Running {
            if ticks == 3 {
                state = State.Attacking
            }
        } else if state == State.Attacking {
            state = State.Dead
        } else if state == State.Dead {
            print("game over\n")
            break
        }

        ticks++
    }

    return 0
}
```

---

## Memory

```opus
function int main() {
    alloc_console()

    let buf = malloc(64)

    mem_write(buf, 0xDEADBEEFCAFEBABE)
    mem_write_i32(buf + 8, 0x1337)
    mem_write_i16(buf + 12, 0xFF)
    mem_write_i8(buf + 14, 42)

    print("64-bit: ") print_hex(mem_read(buf))
    print("32-bit: ") print_hex(mem_read_i32(buf + 8))
    print("16-bit: ") print_int(mem_read_i16(buf + 12))
    print("8-bit:  ") print_int(mem_read_i8(buf + 14))

    let copy = malloc(64)
    memcpy(copy, buf, 64)

    let same = memcmp(buf, copy, 64)
    if same == 0 {
        print("\nbuffers match after memcpy\n")
    }

    free(buf)
    free(copy)
    return 0
}
```

---

## Concurrency

```opus
function int compute(int x) {
    sleep(50)
    return x * x
}

function int main() {
    alloc_console()

    // spawn individual threads
    print("--- spawn/await ---\n")
    let h1 = spawn compute(3)
    let h2 = spawn compute(7)
    let h3 = spawn compute(11)

    let r1 = await h1
    let r2 = await h2
    let r3 = await h3

    print("3^2 = ") print_int(r1)
    print("7^2 = ") print_int(r2)
    print("11^2 = ") print_int(r3)

    // parallel sum with atomics
    print("\n--- parallel for ---\n")
    let sum = malloc(8)
    mem_write(sum, 0)

    parallel for i in range(1, 101) {
        atomic_add(sum, i)
    }

    print("sum 1..100 = ")
    print_int(mem_read(sum))

    free(sum)
    return 0
}
```

---

## Benchmark

```opus
function int burn() {
    var x = 1
    for i in range(0, 10000000) {
        x = x + i * 3 + 7
    }
    return x
}

function int main() {
    alloc_console()
    print("=== parallel benchmark ===\n")
    print("8 iterations, 10M ops each\n\n")

    let t0 = get_tick_count()
    for i in range(0, 8) {
        burn()
    }
    let seq = get_tick_count() - t0
    print("sequential: ")
    print_int(seq)
    print(" ms\n")

    let t1 = get_tick_count()
    parallel for i in range(0, 8) {
        burn()
    }
    let par = get_tick_count() - t1
    print("parallel:   ")
    print_int(par)
    print(" ms\n")

    if par > 0 {
        print("speedup:    ")
        print_int(seq / par)
        print("x\n")
    }

    return 0
}
```

---

## FFI

```opus
function int main() {
    alloc_console()

    let kernel32 = get_module("kernel32.dll")
    print("kernel32: ")
    print_hex(kernel32)

    let get_pid = get_proc(kernel32, "GetCurrentProcessId")
    let pid = ffi_call0(get_pid)
    print("pid: ")
    print_int(pid)

    let user32 = load_library("user32.dll")
    let msgbox_fn = get_proc(user32, "MessageBoxA")
    ffi_call4(msgbox_fn, 0, "Hello from Opus!", "FFI Demo", 0x40)

    print("done\n")
    return 0
}
```

---

## Scanner

A DLL that scans the host process memory for byte patterns:

```opus
// compile with: opus --dll scanner.op

function int main() {
    alloc_console()
    set_title("Opus Scanner")

    let base = get_module(0)
    print("module base: ")
    print_hex(base)

    let pid = get_current_process_id()
    print("process id: ")
    print_int(pid)

    let e_lfanew = mem_read_i32(base + 0x3C)
    let image_size = mem_read_i32(base + e_lfanew + 0x50)
    print("image size: ")
    print_hex(image_size)

    print("\nscanning...\n")
    let t1 = get_tick_count()

    let result = scan(base, image_size, "48 89 5C 24 ? 55 56 57")

    let elapsed = get_tick_count() - t1
    if result != 0 {
        print("found: ")
        print_hex(result)
    } else {
        print("not found\n")
    }

    print("scan took ")
    print_int(elapsed)
    print(" ms\n")

    return 0
}
```

---

## See Also

- [Getting Started](getting-started.md) — installation and first program
- [Language Reference](reference.md) — complete syntax reference
- [Built-in Functions](builtins.md) — all available builtins
- [DLL Mode](dll.md) — generating injectable DLLs
- [Concurrency](concurrency.md) — threading in depth
